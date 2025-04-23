#include "http_conn.h"
#include <map>
#include <stdexcept>
#include <list>
#include <regex>
#include <mysql/mysql.h>
#include <hiredis/hiredis.h>
#include <fmt/format.h>
#include <fstream>

#include "../log/log.h"
#include "../utils/utils/apikey.h"
#include "../utils/utils/file.h"
#include "../utils/nolhmann/json.h"
#include "../utils/utils/mq.h"
#include "./stream.h"
#include "../CGIrabbitmq/rabbitmq_connection_pool.h"
#include "../CGIuploadfile/uploadfile_connection_pool.h"
#include "../CGIgetparseresult/getparseresult_connection_pool.h"
#include "../llm/kimi/files/files.h"
#include "../utils/utils/email.h"

// #define connfdET //边缘触发非阻塞
#define connfdLT // 水平触发阻塞

// #define listenfdET //边缘触发非阻塞
#define listenfdLT // 水平触发阻塞

// 定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";
const char *error_401_title = "Unauthentication";
const char *error_401_form = "Invalid authentication credentials.\n"; // HTTP_401_UNAUTHORIZED
const char *error_429_title = "TOO_MANY_REQUESTS";
const char *error_429_form = "The API Key is currently busy processing requests. Please try again later.\n"; // HTTP_429_TOO_MANY_REQUESTS
const char *error_422_title = "TOO_MANY_REQUESTS";
const char *error_422_form = "Invalid model value. Allowed values are \"deepseek-v3\", \"deepseek-r1\".\n"; // HTTP_422_UNPROCESSABLE_ENTITY
const char *error_413_title = "PAYLOAD_TOO_LARGE";
const char *error_413_form = "{\"error\":{\"message\":\"upload file max limit is 10MB\"}}.\n"; // HTTP_413_Payload_Too_Large

// 当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
const char *doc_root = "/home/wqt/projects/cpp/chat_with_deepseek/root";

// 将表中的用户名和密码放入map
map<string, string> users; // 全局变量，所有的http_conn对象共享一份，只用初始化一份
vector<string> apikeys; // 
locker m_lock;

void http_conn::initmysql_result(connection_pool *connPool)
{
    // 先从连接池中取一个连接
    MYSQL *mysql = NULL;
    connectionRAII mysqlcon(&mysql, connPool);

    // 在user表中检索username，passwd数据，浏览器端输入
    //  if (mysql_query(mysql, "SELECT username,passwd FROM user"))
    if (mysql_query(mysql, "SELECT email,password,apikey FROM users"))
    {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    // 从表中检索完整的结果集
    MYSQL_RES *result = mysql_store_result(mysql);

    // 返回结果集中的列数
    int num_fields = mysql_num_fields(result);

    // 返回所有字段结构的数组
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    // 从结果集中获取下一行，将对应的用户名和密码，存入map中
    while (MYSQL_ROW row = mysql_fetch_row(result))
    {
        string temp1(row[0]);
        string temp2(row[1]);
        users[temp1] = temp2;

        apikeys.push_back(row[2]);
    }
    std::cout << "apikeys: " << apikeys.size() << std::endl;
    std::cout << "users: " << users.size() << std::endl;

    mysql_free_result(result);
}

void http_conn::initredis_result(redis_connection_pool *connPool)
{
    // 先从连接池中取一个连接
    redisContext *redis = NULL;
    redis_connectionRAII redislcon(&redis, connPool);
    // 选择指定的数据库

    

    redisReply *reply = NULL;

    // 执行 FLUSHDB 命令清除 db0
    reply = static_cast<redisReply *>(redisCommand(redis, "FLUSHDB"));
    if (reply == nullptr) {
        std::cerr << "Error executing FLUSHDB command" << std::endl;
    } else {
        if (reply->type == REDIS_REPLY_STATUS && std::string(reply->str) == "OK") {
            std::cout << "db0 has been cleared successfully" << std::endl;
        } else {
            std::cerr << "Error: " << reply->str << std::endl;
        }
    }

    for (int i = 0; i < apikeys.size(); i++)
    {
        std::string apikey = apikeys[i];
        // 执行 Redis 命令
        reply = static_cast<redisReply *>(redisCommand(redis, "SET %s %d", apikey.c_str(), 0));
        if (reply == nullptr)
        {
            std::cerr << "Error executing command" << std::endl;
        }
    }
    // std::cerr << reply << std::endl;
    if (reply != NULL && reply != nullptr)
    {
        freeReplyObject(reply);
    }
    // freeReplyObject(reply);
    // std::cerr << "111" << std::endl;
}

// 对文件描述符设置非阻塞
int setnonblocking(int fd)
{
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

// 将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
void addfd(int epollfd, int fd, bool one_shot)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

#ifdef listenfdET
    event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
#endif

#ifdef listenfdLT
    event.events = EPOLLIN | EPOLLRDHUP;
#endif

    if (one_shot)
        event.events |= EPOLLONESHOT;
    epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
    setnonblocking(fd);
}

// 从内核时间表删除描述符
void removefd(int epollfd, int fd)
{
    epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

// 将事件重置为EPOLLONESHOT
void modfd(int epollfd, int fd, int ev)
{
    epoll_event event;
    event.data.fd = fd;

#ifdef connfdET
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
#endif

#ifdef connfdLT
    event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
#endif

    epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
}

int http_conn::m_user_count = 0;
int http_conn::m_epollfd = -1;

// 关闭连接，关闭一个连接，客户总量减一
void http_conn::close_conn(bool real_close)
{
    if (real_close && (m_sockfd != -1))
    {
        removefd(m_epollfd, m_sockfd);
        m_sockfd = -1;
        m_user_count--;
    }
}

// 初始化连接,外部调用初始化套接字地址
void http_conn::init(int sockfd, const sockaddr_in &addr)
{
    m_sockfd = sockfd;
    m_address = addr;
    // int reuse=1;
    // setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
    addfd(m_epollfd, sockfd, true);
    m_user_count++;
    init();
}

// 初始化新接受的连接
// check_state默认为分析请求行状态
void http_conn::init()
{
    // mysql = NULL;
    // redis = NULL;
    bytes_to_send = 0;
    bytes_have_send = 0;
    m_check_state = CHECK_STATE_REQUESTLINE;
    m_linger = false;
    m_method = GET;
    m_url = 0;
    m_version = 0;
    m_content_length = 0;
    m_host = 0;
    // m_content_type = 0;
    // m_form_data_boundary = 0;
    if (!m_authorization)
    {
        m_authorization = 0;
    };
    if (!m_apikey)
    {
        m_apikey = 0;
    };
    // m_authorization = 0;
    // m_apikey = 0;
    m_start_line = 0;
    m_checked_idx = 0;
    m_read_idx = 0;
    m_write_idx = 0;
    cgi = 0;
    // memset(m_read_buf, '\0', READ_BUFFER_SIZE);
    // memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    memset(m_real_file, '\0', FILENAME_LEN);
    // if(!m_read_buf){
    //     m_read_buf = new char[READ_BUFFER_SIZE];
    // }
    // if(!m_write_buf){
    //     // m_write_buf = new char[WRITE_BUFFER_SIZE];
    //     memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
    // }
}

// 从状态机，用于分析出一行内容
// 返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
http_conn::LINE_STATUS http_conn::parse_line()
{
    if (m_content_type && m_content_length > 2048 && (strncasecmp(m_content_type, "multipart/form-data", 19) == 0) && m_check_state == CHECK_STATE_CONTENT)
    {
        // 这里到了接收表单内容，不再解析行
        return LINE_OPEN;
    }
    else
    {
        char temp;
        for (; m_checked_idx < m_read_idx; ++m_checked_idx)
        {
            temp = m_read_buf[m_checked_idx];
            if (temp == '\r')
            {
                if ((m_checked_idx + 1) == m_read_idx)
                {
                    return LINE_OPEN;
                }
                else if (m_read_buf[m_checked_idx + 1] == '\n')
                {
                    m_read_buf[m_checked_idx++] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
                    return LINE_OK;
                }
                return LINE_BAD;
            }
            else if (temp == '\n')
            {
                if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
                {
                    m_read_buf[m_checked_idx - 1] = '\0';
                    m_read_buf[m_checked_idx++] = '\0';
                    return LINE_OK;
                }
                return LINE_BAD;
            }
        }
    }
    return LINE_OPEN;
}

// 循环读取客户数据，直到无数据可读或对方关闭连接
// 非阻塞ET工作模式下，需要一次性将数据读完
bool http_conn::read_once()
{
    if (m_read_idx >= READ_BUFFER_SIZE)
    {
        return false;
    }
    int bytes_read = 0;

#ifdef connfdLT
    bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

    if (bytes_read <= 0)
    {
        return false;
    }

    m_read_idx += bytes_read;
    return true;

#endif

#ifdef connfdET
    while (true)
    {
        bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
        if (bytes_read == -1)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            return false;
        }
        else if (bytes_read == 0)
        {
            return false;
        }
        m_read_idx += bytes_read;
    }
    return true;
#endif
}

// 解析http请求行，获得请求方法，目标url及http版本号
http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
{
    m_url = strpbrk(text, " \t");
    if (!m_url)
    {
        return BAD_REQUEST;
    }
    *m_url++ = '\0'; // 分割字符串 m_url的位置是:" / HTTP/1.1"相当于把起始空格值为\0， text由"GET / HTTP/1.1" 变为"GET", m_url变为"/ HTTP/1.1"
    char *method = text;

    if (strcasecmp(method, "GET") == 0)
    {
        m_method = GET;
    }
    else if (strcasecmp(method, "POST") == 0)
    {
        m_method = POST;
        cgi = 1;
    }
    else
        return BAD_REQUEST;
    m_url += strspn(m_url, " \t");
    m_version = strpbrk(m_url, " \t");
    if (!m_version)
        return BAD_REQUEST;
    *m_version++ = '\0';
    m_version += strspn(m_version, " \t");
    if (strcasecmp(m_version, "HTTP/1.1") != 0)
        return BAD_REQUEST;
    if (strncasecmp(m_url, "http://", 7) == 0)
    {
        m_url += 7;
        m_url = strchr(m_url, '/');
    }

    if (strncasecmp(m_url, "https://", 8) == 0)
    {
        m_url += 8;
        m_url = strchr(m_url, '/');
    }
    if (!m_url || m_url[0] != '/')
        return BAD_REQUEST;
    // 当url为/时，显示判断界面
    //  if (strlen(m_url) == 1)
    //      strcat(m_url, "judge.html");
    m_check_state = CHECK_STATE_HEADER;
    return NO_REQUEST;
}

// 解析http请求的一个头部信息
http_conn::HTTP_CODE http_conn::parse_headers(char *text)
{
    if (text[0] == '\0')
    {
        if (m_content_length != 0)
        {

            m_check_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    }
    else if (strncasecmp(text, "Connection:", 11) == 0)
    {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0)
        {
            m_linger = true;
        }
    }
    else if (strncasecmp(text, "Content-length:", 15) == 0)
    {
        text += 15;
        text += strspn(text, " \t");
        m_content_length = atol(text);
    }
    else if (strncasecmp(text, "Content-Type:", 13) == 0)
    {
        text += 13;
        text += strspn(text, " \t");
        m_content_type = text;
        if ((strncasecmp(m_content_type, "multipart/form-data; boundary=", 30) == 0))
        {
            m_content_type[19] = '\0';
            m_form_data_boundary = m_content_type + 30;
            // if((strncasecmp(m_content_type, "multipart/form-data; boundary=--", 32) == 0)){
            //     m_content_type[19] = '\0';
            //     m_form_data_boundary = m_content_type + 32; // 去掉前面 --
            // }else{
            //     m_content_type[19] = '\0';
            //     m_form_data_boundary = m_content_type + 30;
            // }
        }
    }
    else if (strncasecmp(text, "Host:", 5) == 0)
    {
        text += 5;
        text += strspn(text, " \t");
        m_host = text;
    }
    else if (strncasecmp(text, "Authorization:", 14) == 0 || strncasecmp(text, "authorization:", 14) == 0)
    {
        text += 14;
        text += strspn(text, " \t");
        m_authorization = text;
    }
    else
    {
        LOG_INFO("oop!unknow header: %s", text);
        Log::get_instance()->flush();
    }
    return NO_REQUEST;
}

// 判断http请求是否被完整读入
http_conn::HTTP_CODE http_conn::parse_content(char *text)
{
    if (m_read_idx >= (m_content_length + m_checked_idx) && !(strncasecmp(m_content_type, "multipart/form-data", 19) == 0))
    {
        text[m_content_length] = '\0';
        // POST请求中最后为输入的用户名和密码
        m_string = text;
        return GET_REQUEST;
    }
    if ((strncasecmp(m_content_type, "multipart/form-data", 19) == 0) && m_read_idx == m_checked_idx + m_content_length)
    {
        m_string = m_read_buf + m_checked_idx;
        return GET_REQUEST;
    }

    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::process_read()
{
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
    {
        text = get_line();

        m_start_line = m_checked_idx;
        // if(strlen(text) > 1024 && m_content_type && !(strncasecmp(m_content_type, "multipart/form-data", 19)==0)){
        //     char truncatedText[1025]; // 创建一个能容纳1024个字符加字符串结束符的数组
        //     // 复制前1021个字符到新数组
        //     std::strncpy(truncatedText, text, 1021);
        //     // 添加 ...
        //     std::strcpy(truncatedText + 1021, "...");
        //     truncatedText[1024] = '\0'; // 手动添加字符串结束符
        //     LOG_INFO("%s", truncatedText);
        //     Log::get_instance()->flush();
        // }else{
        //     LOG_INFO("%s", text);
        //     Log::get_instance()->flush();
        // }

        switch (m_check_state)
        {
        case CHECK_STATE_REQUESTLINE:
        {
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            break;
        }
        case CHECK_STATE_HEADER:
        {
            ret = parse_headers(text);
            if (ret == BAD_REQUEST)
                return BAD_REQUEST;
            else if (ret == GET_REQUEST)
            {
                return do_request();
            }
            break;
        }
        case CHECK_STATE_CONTENT:
        {
            ret = parse_content(text);
            if (ret == GET_REQUEST)
                return do_request();
            line_status = LINE_OPEN;
            break;
        }
        default:
            return INTERNAL_ERROR;
        }
    }
    return NO_REQUEST;
}

http_conn::HTTP_CODE http_conn::do_request()
{
    strcpy(m_real_file, doc_root);
    int len = strlen(doc_root);
    // const char *p = strrchr(m_url, '/');
    const char *p = m_url;
    // 处理cgi
    if (cgi == 1)
    {
        // 根据标志判断是登录检测还是注册检测
        char flag = m_url[1];

        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/");
        strcat(m_url_real, m_url + 2);
        strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
        free(m_url_real);

        char name[100], password[100], apikey[100];

        // 同步线程登录校验
        //  if (*(p + 1) == '3')
        if (m_method == POST && strncasecmp(p + 1, "register", 8) == 0)
        {
            nlohmann::json request_json = nlohmann::json::parse(m_string);
            std::string email_or_phone = request_json["email_or_phone"].get<std::string>();
            std::string password = request_json["password"].get<std::string>();
            std::string valid_code = request_json["valid_code"].get<std::string>();

            // 检查redis中verification_code是否过期
            redis_connectionRAII rediscon(&redis, m_redis_pool);
            nlohmann::json json1;
            m_lock.lock();
            redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "SELECT %d", 1));
            if (reply == nullptr)
            {
                std::cerr << "执行 SELECT 命令时出错" << std::endl;
                json1["isok"] = false;
                json1["msg"] = "执行 SELECT 命令时出错";
            }
            reply = static_cast<redisReply *>(redisCommand(redis, "GET %s", email_or_phone.c_str()));

            if (reply == nullptr)
            {
                std::cerr << "Error executing command" << std::endl;
                json1["isok"] = false;
                json1["msg"] = "执行 SELECT 命令时出错";
            }
            if (reply->type == REDIS_REPLY_NIL)
            {
                std::cerr << "No this apikey" << std::endl;
                json1["isok"] = false;
                json1["msg"] = "验证码已经过期了";
            }
            // json1["isok"] = true;
            // json1["msg"] = "验证成功";
            std::string v_code_true = reply->str;
            if (v_code_true != valid_code)
            {
                json1["isok"] = false;
                json1["msg"] = "验证码不正确";
            }
            reply = static_cast<redisReply *>(redisCommand(redis, "SELECT %d", 0));
            m_lock.unlock();

            // 释放回复对象
            freeReplyObject(reply);

            if (!json1.contains("isok"))
            { // 前面没有出错才继续执行后面逻辑
                // 如果是注册，先检测数据库中是否有重名的
                // 没有重名的，进行增加数据
                char *sql_insert = (char *)malloc(sizeof(char) * 200);
                // strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
                gen_api_key(apikey);
                strcpy(sql_insert, "INSERT INTO users(email, password, apikey) VALUES(");
                strcat(sql_insert, "'");
                strcat(sql_insert, email_or_phone.c_str());
                strcat(sql_insert, "', '");
                strcat(sql_insert, password.c_str());
                strcat(sql_insert, "', '");
                strcat(sql_insert, apikey);
                strcat(sql_insert, "')");

                for (const auto &pair : users)
                {
                }

                // nlohmann::json json1;

                if (users.find(email_or_phone.c_str()) == users.end())
                {

                    connectionRAII mysqlcon(&mysql, m_mysql_pool);
                    int ret = 1;
                    if ((ret = mysql_ping(mysql)) != 0)
                    {
                        std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                        mysql = NULL;
                        mysql = m_mysql_pool->get_conn();
                        mysqlcon.updateConRAII(&mysql); // 这里必须更新
                    }
                    mysql_query(mysql, "set names utf8mb4");
                    int res = mysql_query(mysql, sql_insert);
                    // users.insert(pair<string, string>(name, password));
                    m_lock.unlock();
                    free(sql_insert);

                    if (!res)
                    {
                        users.insert(pair<string, string>(email_or_phone, password));
                        json1["isok"] = true;
                        json1["msg"] = "注册成功";
                        m_lock.lock();
                        redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "SET %s %d", apikey, 0));
                        m_lock.unlock();
                        freeReplyObject(reply);
                    }
                    else
                    {
                        json1["isok"] = false;
                        json1["msg"] = "注册失败";
                    }
                }
                else
                {
                    // strcpy(m_url, "/registerError.html");
                    json1["isok"] = false;
                    json1["msg"] = "该邮箱已经注册过";
                }
            }
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            // 分配新的内存
            std::string json_str = json1.dump();
            m_response_str = new char[json_str.length() + 1];

            // 复制字符串内容
            std::strcpy(m_response_str, json_str.c_str());
            // strcpy(m_url, "/welcome.html");
            // return STRING_REQUEST;
            return JSON_REQUEST;
            return JSON_REQUEST;
        }
        // 如果是登录，直接判断
        // 若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
        //  else if (*(p + 1) == '2')
        else if (m_method == POST && strncasecmp(p + 1, "login", 5) == 0)
        {

            nlohmann::json request_json = nlohmann::json::parse(m_string);
            std::string email_or_phone = request_json["email_or_phone"].get<std::string>();
            std::string password = request_json["password"].get<std::string>();
            std::string login_type = request_json["login_type"].get<std::string>();

            if (users.find(email_or_phone.c_str()) != users.end() && users[email_or_phone] == password)
            {
                char *sql_find = (char *)malloc(sizeof(char) * 200);
                strcpy(sql_find, "SELECT apikey, id FROM users WHERE email = '");
                strcat(sql_find, email_or_phone.c_str());
                strcat(sql_find, "' AND password = '");
                strcat(sql_find, password.c_str());
                strcat(sql_find, "'");
                connectionRAII mysqlcon(&mysql, m_mysql_pool);
                int ret = 1;
                if ((ret = mysql_ping(mysql)) != 0)
                {
                    std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                    mysql = NULL;
                    mysql = m_mysql_pool->get_conn();
                    mysqlcon.updateConRAII(&mysql); // 这里必须更新
                }
                mysql_query(mysql, "set names utf8mb4");
                if (mysql_query(mysql, sql_find))
                {
                    LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
                }

                // 从表中检索完整的结果集
                MYSQL_RES *result = mysql_store_result(mysql);

                // 从结果集中获取下一行，将对应的用户名和密码，存入map中
                MYSQL_ROW row = mysql_fetch_row(result);
                string _apikey(row[0]);
                int user_id = std::stoi(row[1]);
                // string content = generateAuthorizedMessage(_apikey);
                // string r_json_str1 = "{\"isok\":\"false\", \"apikey\":\"\", \"msg\":\"用户名或密码错误\"}";
                string r_json_str2 = R"(
                    "isok" : true,
                    "apikey": "{}",
                    "userid": {},
                    "msg": ""
                )";
                string r_json_str1 = '{' + fmt::format(r_json_str2, _apikey, user_id) + '}';
                string r_json_str = nlohmann::json::parse(r_json_str1).dump();
                if (m_response_str)
                {
                    delete[] m_response_str;
                }
                // 分配新的内存
                m_response_str = new char[r_json_str.length() + 1];

                // 复制字符串内容
                std::strcpy(m_response_str, r_json_str.c_str());
                // strcpy(m_url, "/welcome.html");
                // return STRING_REQUEST;
                return JSON_REQUEST;
            }
            else
            {
                string r_json_str1 = "{\"isok\":false, \"apikey\":\"\", \"msg\":\"用户名或密码错误\"}";
                string r_json_str = nlohmann::json::parse(r_json_str1).dump();
                // strcpy(m_url, "/logError.html");
                if (m_response_str)
                {
                    delete[] m_response_str;
                }
                m_response_str = new char[r_json_str.length() + 1];
                std::strcpy(m_response_str, r_json_str.c_str());
                return JSON_REQUEST;
            }
        }
        else if (m_method == POST && strncasecmp(p + 1, "sned_verification_code", 22) == 0)
        {
            nlohmann::json data = nlohmann::json::parse(m_string);
            std::string email = data["email"].get<std::string>();
            // 检查一下是否已经注册过这个email
            connectionRAII mysqlcon(&mysql, m_mysql_pool);
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }
            m_lock.lock();
            mysql_query(mysql, "set names utf8mb4");
            std::string sql_select1 = "SELECT * FROM users WHERE email='{}'";
            std::string sql_select = fmt::format(sql_select1, email);
            mysql_query(mysql, sql_select.c_str());

            MYSQL_RES *result = mysql_store_result(mysql);
            int num_rows = mysql_num_rows(result);
            nlohmann::json json1;
            if (num_rows > 0)
            {
                json1["isok"] = false;
                json1["msg"] = "该邮箱已经注册过";
            }
            else
            {
                mysql_free_result(result);

                bool isok = sendEmail(email);
                if (!isok)
                {
                    json1["isok"] = false;
                    json1["msg"] = "发送失败";
                }
                else
                {
                    json1["isok"] = true;
                    json1["msg"] = "发送成功";
                }
            }
            m_lock.unlock();
            std::string res1 = json1.dump();
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            m_response_str = new char[res1.length() + 1];
            std::strcpy(m_response_str, res1.c_str());

            return JSON_REQUEST;
        }
        else if (m_method == POST && strncasecmp(p + 1, "v1/chat/completions", 19) == 0)
        {
            // 首先验证apikey
            if (m_authorization)
            {
                if (strlen(m_authorization) == 0 || !(strncasecmp(m_authorization, "Bearer ", 7) == 0))
                {
                    return UNAUTHORIZED_ERROR;
                }
                else
                {
                    m_authorization += 7;

                    // 深拷贝
                    m_apikey = strdup(m_authorization);
                    if (m_apikey == nullptr)
                    {
                        // 处理内存分配失败
                    }

                    // 如果是一个线程分段锁，就不能再threadpool初始化，不然导致死锁
                    redis_connectionRAII redislcon(&redis, m_redis_pool);
                    redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "GET %s", m_apikey));

                    if (reply == nullptr)
                    {
                        std::cerr << "Error executing command" << std::endl;
                        return UNAUTHORIZED_ERROR;
                    }
                    if (reply->type == REDIS_REPLY_NIL)
                    {
                        std::cerr << "No this apikey 1" << std::endl;
                        return UNAUTHORIZED_ERROR;
                    }

                    if (reply->type == REDIS_REPLY_STRING)
                    {
                        int value = std::stoi(reply->str);

                        // if (value >= m_apikey_max)
                        if (false)
                        {
                            std::cerr << "超过MAX" << std::endl;
                            return TOO_MANY_REQUESTS_ERROR;
                        }
                        else
                        {
                            value++;
                            reply = static_cast<redisReply *>(redisCommand(redis, "SET %s %d", m_apikey, value));
                            if (reply == nullptr)
                            {
                                std::cerr << "Error executing command" << std::endl;
                                return UNAUTHORIZED_ERROR;
                            }
                        }
                    }

                    // 释放回复对象
                    freeReplyObject(reply);
                    // m_lock.unlock();
                }
            }
            else
            {
                return UNAUTHORIZED_ERROR;
            }

            // 定义 JSON 响应体字符串
            nlohmann::json data = nlohmann::json::parse(m_string);
            std::vector<nlohmann::json> contents = data["messages"].get<std::vector<nlohmann::json>>();
            std::string model = data["model"].get<std::string>();
            std::string chat_id = data["chat_id"].get<std::string>();
            int user_id = data["user_id"].get<int>();
            int msg_idx = data["msg_idx"].get<int>();
            int idx = data["idx"].get<int>();
            bool is_new = data["is_new"].get<bool>();
            bool is_replay = data["is_replay"].get<bool>();
            nlohmann::json file_refs = data["file_refs"].get<nlohmann::json>();
            bool search = false;
            if (data.contains("search") && data["search"].is_boolean())
            {
                search = data["search"].get<bool>();
            }
            if (model != "deepseek-v3" && model != "deepseek-r1")
            {
                m_lock.lock();
                dcr_apikey(redis, m_apikey);
                m_lock.unlock();
                free(m_apikey);
                return UNPROCESSABLE_ENTITY_ERROR;
            }
            // m_lock.lock();
            // 这里使用完redis后先手动释放,因为后续chat是耗时操作
            // m_redis_pool->ReleaseConnection(redis);
            // std::destroy_at(redislcon)
            // 这里还要释放一下锁，不然后续依然没法获取redis
            // m_lock.unlock();
            // 这里不用手动释放，是因为if中定义的变量再出if后会自动析构。

            std::string content = contents[0]["content"].get<std::string>();
            m_token = m_token_pool->GetToken();
            std::string response_str; // 记录响应消息
            chat(content, model, m_token, m_epollfd, m_sockfd, response_str, search);
            m_token_pool->ReleaseToken(m_token);

            while (true)
            {
                m_lock.lock();
                if (m_redis_pool->GetFreeConn() <= 0)
                {
                    // 这里释放锁，避免死锁
                    m_lock.unlock();
                    sleep(1);
                    continue;
                }
                // 这里再获取一个redis
                redis_connectionRAII redislcon(&redis, m_redis_pool);
                dcr_apikey(redis, m_apikey);
                m_lock.unlock();
                break;
            }

            free(m_apikey);

            // 最终要将消息记录存到数据库:
            // 如果chat_id为“”表示是新的对话
            // if(chat_id.size()==0){
            //     chat_id = gen_uuid();
            // }
            connectionRAII mysqlcon(&mysql, m_mysql_pool);
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }
            m_lock.lock();
            std::string create_time = getMySQLDateTime();
            mysql_query(mysql, "set names utf8mb4");
            if (is_new)
            {
                std::string sql_insert1 = "INSERT INTO chats(chat_id, chat_name, create_time, update_time, user_id) VALUES ";
                std::string values_template = "('{}','{}','{}','{}',{})";
                std::string chat_name1;
                // chat_id = gen_uuid();
                if (content.size() > 10)
                {
                    chat_name1 = content.substr(0, 10);
                }
                else
                {
                    chat_name1 = content;
                }
                std::string chat_name = removeEmojis(chat_name1);
                std::string values1 = fmt::format(values_template, chat_id, removeEmojis(content), create_time, create_time, user_id); // 用户提问
                std::string sql_insert = sql_insert1 + values1;
                
                int res = mysql_query(mysql, sql_insert.c_str());
            }
            else
            {
                std::string sql_update1 = "UPDATE chats SET update_time = '{}' WHERE chat_id = '{}'";
                std::string sql_update = fmt::format(sql_update1, create_time, chat_id); // 用户提问
                int res = mysql_query(mysql, sql_update.c_str());
            }

            if (is_replay)
            {
                std::string sql_update1 = "UPDATE messages SET msg = '{}', update_time = '{}' WHERE chat_id = '{}' AND msg_idx = {}";
                msg_idx = msg_idx + 1;
                std::string sql_update = fmt::format(sql_update1, removeEmojis(response_str), create_time, chat_id, msg_idx); // 只更新assistant回答
                int res = mysql_query(mysql, sql_update.c_str());
                // std::cout << sql_update <<std::endl;
                // std::cout << ret <<std::endl;
            }
            else
            {
                std::string sql_insert1 = "INSERT INTO messages(chat_id, msg_idx, idx, file_refs, user_id, create_time, update_time, msg) VALUES ";
                std::string values_template = "('{}',{},{},'{}',{},'{}','{}','{}')";
                std::string values1 = fmt::format(values_template, chat_id, msg_idx, idx, file_refs.dump(), user_id, create_time, create_time, removeEmojis(content)); // 用户提问
                msg_idx = msg_idx + 1;
                std::string values2 = fmt::format(values_template, chat_id, msg_idx, idx, file_refs.dump(), user_id, create_time, create_time, removeEmojis(response_str)); // assistant回答
                std::string sql_insert = sql_insert1 + values1 + ", " + values2;
                int res = mysql_query(mysql, sql_insert.c_str());
            }
            m_lock.unlock();

            return NO_REQUEST;
        }
        else if (m_method == POST && strncasecmp(p + 1, "v1/chat/test_completions", 24) == 0)
        {
            // 首先验证apikey
            if (m_authorization)
            {
                if (strlen(m_authorization) == 0 || !(strncasecmp(m_authorization, "Bearer ", 7) == 0))
                {
                    return UNAUTHORIZED_ERROR;
                }
                else
                {
                    m_authorization += 7;

                    // 深拷贝
                    // m_lock.lock();
                    // m_apikey = strdup(m_authorization);
                    // m_lock.unlock();
                    m_apikey = m_authorization;
                    if (m_apikey == nullptr)
                    {
                        // 处理内存分配失败
                    }

                    // 如果是一个线程分段锁，就不能再threadpool初始化，不然导致死锁
                    redis_connectionRAII redislcon(&redis, m_redis_pool);

                    m_lock.lock();
                    redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "GET %s", m_apikey));

                    if (reply == nullptr)
                    {
                        m_lock.unlock();
                        std::cerr << "Error executing command" << std::endl;
                        return UNAUTHORIZED_ERROR;
                    }
                    if (reply->type == REDIS_REPLY_NIL)
                    {
                        m_lock.unlock();
                        std::cerr << "No this apikey 1" << std::endl;
                        return UNAUTHORIZED_ERROR;
                    }

                    // if (reply->type == REDIS_REPLY_STRING)
                    // {
                    //     int value = std::stoi(reply->str);

                    //     // if (value >= m_apikey_max)
                    //     if (false)
                    //     {
                    //         m_lock.unlock();
                    //         std::cerr << "超过MAX" << std::endl;
                    //         return TOO_MANY_REQUESTS_ERROR;
                    //     }
                    //     else
                    //     {
                    //         // 这里也要加锁啊
                    //         // m_lock.lock();
                    //         value++;
                    //         reply = static_cast<redisReply *>(redisCommand(redis, "SET %s %d", m_apikey, value));
                    //         m_lock.unlock();
                    //         if (reply == nullptr)
                    //         {
                    //             std::cerr << "Error executing command" << std::endl;
                    //             return UNAUTHORIZED_ERROR;
                    //         }
                    //     }
                    // }
                    m_lock.unlock();

                    // 释放回复对象
                    freeReplyObject(reply);
                    // m_lock.unlock();
                }
            }
            else
            {
                return UNAUTHORIZED_ERROR;
            }

            // 定义 JSON 响应体字符串
            nlohmann::json data = nlohmann::json::parse(m_string);
            std::vector<nlohmann::json> contents = data["messages"].get<std::vector<nlohmann::json>>();
            std::string model = data["model"].get<std::string>();
            std::string chat_id = data["chat_id"].get<std::string>();
            int user_id = data["user_id"].get<int>();
            int msg_idx = data["msg_idx"].get<int>();
            int idx = data["idx"].get<int>();
            bool is_new = data["is_new"].get<bool>();
            bool is_replay = data["is_replay"].get<bool>();
            nlohmann::json file_refs = data["file_refs"].get<nlohmann::json>();
            bool search = false;
            if (data.contains("search") && data["search"].is_boolean())
            {
                search = data["search"].get<bool>();
            }
            if (model != "deepseek-v3" && model != "deepseek-r1")
            {
                // m_lock.lock();
                // dcr_apikey(redis, m_apikey);
                // free(m_apikey);
                // m_lock.unlock();
                
                return UNPROCESSABLE_ENTITY_ERROR;
            }
            // m_lock.lock();
            // 这里使用完redis后先手动释放,因为后续chat是耗时操作
            // m_redis_pool->ReleaseConnection(redis);
            // std::destroy_at(redislcon)
            // 这里还要释放一下锁，不然后续依然没法获取redis
            // m_lock.unlock();
            // 这里不用手动释放，是因为if中定义的变量再出if后会自动析构。

            std::string content = contents[0]["content"].get<std::string>();
            m_token = m_token_pool->GetToken();
            std::string response_str; // 记录响应消息
            chat(content, model, m_token, m_epollfd, m_sockfd, response_str, search);
            m_token_pool->ReleaseToken(m_token);

            while (true)
            {
                m_lock.lock();
                if (m_redis_pool->GetFreeConn() <= 0)
                {
                    // 这里释放锁，避免死锁
                    m_lock.unlock();
                    sleep(1);
                    continue;
                }
                // 这里再获取一个redis
                redis_connectionRAII redislcon(&redis, m_redis_pool);
                // dcr_apikey(redis, m_apikey);
                m_lock.unlock();
                break;
            }

            // m_lock.lock();
            // free(m_apikey);
            // m_lock.unlock();

            // 最终要将消息记录存到数据库:
            // 如果chat_id为“”表示是新的对话
            // if(chat_id.size()==0){
            //     chat_id = gen_uuid();
            // }
            connectionRAII mysqlcon(&mysql, m_mysql_pool);
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }
            m_lock.lock();
            std::string create_time = getMySQLDateTime();
            mysql_query(mysql, "set names utf8mb4");
            
            if (is_new)
            {
                std::string sql_insert1 = "INSERT INTO chats(chat_id, chat_name, create_time, update_time, user_id) VALUES ";
                std::string values_template = "('{}','{}','{}','{}',{})";
                std::string chat_name1;
                // chat_id = gen_uuid();
                if (content.size() > 10)
                {
                    chat_name1 = content.substr(0, 10);
                }
                else
                {
                    chat_name1 = content;
                }
                std::string chat_name = removeEmojis(chat_name1);
                std::string values1 = fmt::format(values_template, chat_id, removeEmojis(content), create_time, create_time, user_id); // 用户提问
                std::string sql_insert = sql_insert1 + values1;
                
                int res = mysql_query(mysql, sql_insert.c_str());
            }
            else
            {
                std::string sql_update1 = "UPDATE chats SET update_time = '{}' WHERE chat_id = '{}'";
                std::string sql_update = fmt::format(sql_update1, create_time, chat_id); // 用户提问
                int res = mysql_query(mysql, sql_update.c_str());
            }

            if (is_replay)
            {
                std::string sql_update1 = "UPDATE messages SET msg = '{}', update_time = '{}' WHERE chat_id = '{}' AND msg_idx = {}";
                msg_idx = msg_idx + 1;
                std::string sql_update = fmt::format(sql_update1, removeEmojis(response_str), create_time, chat_id, msg_idx); // 只更新assistant回答
                int res = mysql_query(mysql, sql_update.c_str());
                // std::cout << sql_update <<std::endl;
                // std::cout << ret <<std::endl;
            }
            else
            {
                std::string sql_insert1 = "INSERT INTO messages(chat_id, msg_idx, idx, file_refs, user_id, create_time, update_time, msg) VALUES ";
                std::string values_template = "('{}',{},{},'{}',{},'{}','{}','{}')";
                std::string values1 = fmt::format(values_template, chat_id, msg_idx, idx, file_refs.dump(), user_id, create_time, create_time, removeEmojis(content)); // 用户提问
                msg_idx = msg_idx + 1;
                std::string values2 = fmt::format(values_template, chat_id, msg_idx, idx, file_refs.dump(), user_id, create_time, create_time, removeEmojis(response_str)); // assistant回答
                std::string sql_insert = sql_insert1 + values1 + ", " + values2;
                int res = mysql_query(mysql, sql_insert.c_str());
            }
            m_lock.unlock();

            return NO_REQUEST;
        }
        else if (m_method == POST && strncasecmp(p + 1, "v1/files", 8) == 0)
        {
            std::string receivedData;
            receivedData.append(m_read_buf + m_checked_idx, m_content_length);

            std::string boundary(m_form_data_boundary);
            std::map<std::string, std::string> result = parseMultipartFormData1(receivedData, boundary);
            std::string filename = result["filename"];
            std::string file = result["file"];
            std::string content_type = result["file_content_type"];
            std::string content = result["file_content"];
            if (content.size() > MAX_UPLOADFILE_SIZE)
            {
                return PAYLOAD_TOO_LARGE_ERROR;
            }

            std::string task_id = gen_uuid();

            uploadfile_connectionRAII uploadlcon(&uploadfileconn, m_uploadfileconn_pool);
            nlohmann::json json_1 = uploadfileconn->upload_file_python_server(task_id, filename, content, content_type);

            std::string json_str = json_1.dump();

            // 动态分配内存
            // 首先释放原有内存
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            // 分配新的内存
            m_response_str = new char[json_str.length() + 1];

            // 复制字符串内容
            std::strcpy(m_response_str, json_str.c_str());
            // 输出转换后的字符串
            return JSON_REQUEST;
        }
        else if (m_method == POST && strncasecmp(p + 1, "change_chat_name", 16) == 0)
        {
            nlohmann::json data = nlohmann::json::parse(m_string);

            std::string chat_id = data["chat_id"].get<std::string>();
            std::string new_chat_name = data["new_chat_name"].get<std::string>();

            connectionRAII mysqlcon(&mysql, m_mysql_pool);
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }
            m_lock.lock();
            std::string create_time = getMySQLDateTime();
            mysql_query(mysql, "set names utf8mb4");
            std::string sql_update1 = "UPDATE chats SET chat_name = '{}', update_time = '{}' WHERE chat_id = '{}'";
            std::string sql_update = fmt::format(sql_update1, new_chat_name, create_time, chat_id); // 用户提问
            int res = mysql_query(mysql, sql_update.c_str());
            m_lock.unlock();

            nlohmann::json json1;
            json1["isok"] = true;
            json1["msg"] = "修改成功";
            std::string json_str = json1.dump();
            // 动态分配内存
            // 首先释放原有内存
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            // 分配新的内存
            m_response_str = new char[json_str.length() + 1];

            // 复制字符串内容
            std::strcpy(m_response_str, json_str.c_str());
            // 输出转换后的字符串
            return JSON_REQUEST;
        }
        else if (m_method == POST && strncasecmp(p + 1, "delete_chat", 11) == 0)
        {
            nlohmann::json data = nlohmann::json::parse(m_string);

            std::string chat_id = data["chat_id"].get<std::string>();

            connectionRAII mysqlcon(&mysql, m_mysql_pool);
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }
            m_lock.lock();
            std::string create_time = getMySQLDateTime();
            mysql_query(mysql, "set names utf8mb4");
            std::string sql_delete1 = "DELETE FROM chats WHERE chat_id = '{}'";
            std::string sql_delete = fmt::format(sql_delete1, chat_id); // 用户提问
            int res = mysql_query(mysql, sql_delete.c_str());

            sql_delete1 = "DELETE FROM messages WHERE chat_id = '{}'";
            sql_delete = fmt::format(sql_delete1, chat_id); // 用户提问
            res = mysql_query(mysql, sql_delete.c_str());
            m_lock.unlock();

            nlohmann::json json1;
            json1["isok"] = true;
            json1["msg"] = "删除成功";
            std::string json_str = json1.dump();
            // 动态分配内存
            // 首先释放原有内存
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            // 分配新的内存
            m_response_str = new char[json_str.length() + 1];

            // 复制字符串内容
            std::strcpy(m_response_str, json_str.c_str());
            // 输出转换后的字符串
            return JSON_REQUEST;
        }
    }

    if (m_method == GET && strncasecmp(p, "/", 1) == 0 && strlen(p) == 1)
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/index.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }

    // if (*(p + 1) == '0')
    else if (m_method == GET && strncasecmp(p + 1, "register", 8) == 0)
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/register.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    // else if (*(p + 1) == '1')
    else if (m_method == GET && strncasecmp(p + 1, "login", 5) == 0)
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/login.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (m_method == GET && strncasecmp(p + 1, "v1/models", 9) == 0)
    {
        // 定义 JSON 响应体字符串
        char *m_response_str1 = R"({
            "data": [
                {
                    "id": "deepseek-v3"
                },
                {
                    "id": "deepseek-r1"
                }
            ]
        }
        )";
        // 解析 JSON 字符串
        nlohmann::json j = nlohmann::json::parse(m_response_str1);

        // 将 JSON 对象转换回字符串
        std::string json_str = j.dump();
        // 动态分配内存
        // 释放原有内存

        if (m_response_str)
        {
            delete[] m_response_str;
        }
        // 分配新的内存
        m_response_str = new char[json_str.length() + 1];

        // 复制字符串内容
        std::strcpy(m_response_str, json_str.c_str());
        // 输出转换后的字符串
        return JSON_REQUEST;
    }
    else if (m_method == GET && strncasecmp(p + 1, "historys", 8) == 0)
    {

        // 首先验证apikey

        if (m_authorization)
        {
            if (strlen(m_authorization) == 0 || !(strncasecmp(m_authorization, "Bearer ", 7) == 0))
            {
                return UNAUTHORIZED_ERROR;
            }
            else
            {
                m_authorization += 7;

                // if(m_apikey) {free(m_apikey);}
                // 深拷贝
                m_apikey = strdup(m_authorization);
                if (m_apikey == nullptr)
                {
                    std::cerr << "Error executing command" << std::endl;
                    // 处理内存分配失败
                }

                // 如果是一个线程分段锁，就不能再threadpool初始化，不然导致死锁
                redis_connectionRAII redislcon(&redis, m_redis_pool);
                redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "GET %s", m_apikey));

                if (reply == nullptr)
                {
                    std::cerr << "Error executing command" << std::endl;
                    return UNAUTHORIZED_ERROR;
                }
                if (reply->type == REDIS_REPLY_NIL)
                {
                    std::cerr << "No this apikey" << std::endl;
                    return UNAUTHORIZED_ERROR;
                }
            }
        }
        else
        {
            std::cerr << "没有验证信息" << std::endl;
            return UNAUTHORIZED_ERROR;
        }
        connectionRAII mysqlcon(&mysql, m_mysql_pool);
        try
        {
            int ret = 1;
            if ((ret = mysql_ping(mysql)) != 0)
            {
                std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
                // mysql_close(mysql); // 这里如果已经断开(服务端释放的),也不能再关闭了
                // std::cout << "断开检查: " << mysql_ping(mysql) << std::endl;
                // mysql_close(mysql);
                // std::cout << "mysql释放了" << std::endl;
                mysql = NULL;
                mysql = m_mysql_pool->get_conn();
                mysqlcon.updateConRAII(&mysql); // 这里必须更新
            }

            ret = mysql_query(mysql, "set names utf8mb4");

            std::string sql_select_userid1 = "SELECT id FROM users WHERE apikey = '{}'";
            std::string sql_select_userid = fmt::format(sql_select_userid1, m_apikey);
            mysql_query(mysql, sql_select_userid.c_str());
            // 从表中检索完整的结果集
            MYSQL_RES *result_userid = mysql_store_result(mysql);
            if (result_userid == NULL) { return UNAUTHORIZED_ERROR; }
            MYSQL_ROW row_userid = mysql_fetch_row(result_userid);
            char *user_id1 = row_userid[0];
            mysql_free_result(result_userid);
            int user_id = std::stoi(user_id1);
            int offset = 0;
            int page_size = 10;
            std::string sql_select1 = "SELECT chat_id, chat_name, update_time FROM chats WHERE user_id = {} ORDER BY update_time DESC LIMIT {}, {}";
            std::string sql_select = fmt::format(sql_select1, user_id, offset, page_size);
            mysql_query(mysql, sql_select.c_str());
            // 从表中检索完整的结果集
            MYSQL_RES *result = mysql_store_result(mysql);
            int num_fields = mysql_num_fields(result);
            // 从结果集中获取下一行，将对应的用户名和密码，存入map中
            vector<nlohmann::json> chats;
            while (MYSQL_ROW row = mysql_fetch_row(result))
            {
                std::string chat_id(row[0]);
                std::string chat_name(row[1]);
                std::string update_time(row[2]);
                nlohmann::json json1;
                json1["chat_id"] = chat_id;
                json1["chat_name"] = chat_name;
                json1["update_time"] = update_time;
                chats.push_back(json1);
            }
            mysql_free_result(result);
            
            nlohmann::json data1;
            data1["data"] = chats;

            // 将 JSON 对象转换回字符串
            std::string json_str = data1.dump();
            // 动态分配内存
            // 释放原有内存
            if (m_response_str)
            {
                delete[] m_response_str;
            }
            // 分配新的内存
            m_response_str = new char[json_str.length() + 1];

            // 复制字符串内容
            std::strcpy(m_response_str, json_str.c_str());
        }
        catch (const std::runtime_error &e)
        {
            m_lock.unlock();
            std::cerr << "Runtime exception caught: " << e.what() << std::endl;
        }
        catch (const std::logic_error &e)
        {
            m_lock.unlock();
            std::cerr << "Logic exception caught: " << e.what() << std::endl;
        }
        catch (...)
        {
            m_lock.unlock();
            std::cerr << "An unknown exception occurred." << std::endl;
        }

        // 输出转换后的字符串
        return JSON_REQUEST;
    }
    else if (m_method == GET && strncasecmp(p + 1, "history_messages", 16) == 0)
    {
        // 首先验证apikey
        if (m_authorization)
        {
            if (strlen(m_authorization) == 0 || !(strncasecmp(m_authorization, "Bearer ", 7) == 0))
            {
                return UNAUTHORIZED_ERROR;
            }
            else
            {
                m_authorization += 7;

                // 深拷贝
                m_apikey = strdup(m_authorization);
                if (m_apikey == nullptr)
                {
                    // 处理内存分配失败
                }

                // 如果是一个线程分段锁，就不能再threadpool初始化，不然导致死锁
                redis_connectionRAII redislcon(&redis, m_redis_pool);
                redisReply *reply = static_cast<redisReply *>(redisCommand(redis, "GET %s", m_apikey));

                if (reply == nullptr)
                {
                    std::cerr << "Error executing command" << std::endl;
                    return UNAUTHORIZED_ERROR;
                }
                if (reply->type == REDIS_REPLY_NIL)
                {
                    std::cerr << "No this apikey" << std::endl;
                    return UNAUTHORIZED_ERROR;
                }
            }
        }
        else
        {
            std::cerr << "没有验证信息" << std::endl;
            return UNAUTHORIZED_ERROR;
        }

        std::string url(m_url);
        size_t pos_event = url.find("=");
        std::string chat_id = url.substr(pos_event + 1);

        // 定义 JSON 响应体字符串

        connectionRAII mysqlcon(&mysql, m_mysql_pool);
        int ret = 1;
        if ((ret = mysql_ping(mysql)) != 0)
        {
            std::cout << "mysql连接断, 重新连接: " << ret << std::endl;
            mysql = NULL;
            mysql = m_mysql_pool->get_conn();
            mysqlcon.updateConRAII(&mysql); // 这里必须更新
        }
        mysql_query(mysql, "set names utf8mb4");
        std::string sql_select1 = "SELECT msg_idx, msg FROM messages WHERE chat_id='{}'";
        std::string sql_select = fmt::format(sql_select1, chat_id);
        mysql_query(mysql, sql_select.c_str());

        MYSQL_RES *result = mysql_store_result(mysql);
        int num_fields = mysql_num_fields(result);
        // 从结果集中获取下一行，将对应的用户名和密码，存入map中
        vector<nlohmann::json> messages;
        while (MYSQL_ROW row = mysql_fetch_row(result))
        {
            int msg_idx = std::stoi(row[0]);
            std::string msg(row[1]);
            std::string role;
            if (msg_idx % 2 == 0)
            {
                role = "user";
            }
            else
            {
                role = "assistant";
            }

            nlohmann::json json1;
            json1["msg_idx"] = msg_idx;
            json1["role"] = role;
            json1["msg"] = msg;
            messages.push_back(json1);
        }
        mysql_free_result(result);
        free(m_apikey);
        nlohmann::json data1;
        data1["data"] = messages;

        // 将 JSON 对象转换回字符串
        std::string json_str = data1.dump();
        // 动态分配内存
        // 释放原有内存
        if (m_response_str)
        {
            delete[] m_response_str;
        }
        // 分配新的内存
        m_response_str = new char[json_str.length() + 1];

        // 复制字符串内容
        std::strcpy(m_response_str, json_str.c_str());
        // 输出转换后的字符串
        return JSON_REQUEST;
    }

    else if (m_method == GET && strncasecmp(p + 1, "chat", 4) == 0)
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/chat.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));
        free(m_url_real);
    }
    else if (m_method == GET && strncasecmp(p + 1, "v1/files/", 9) == 0)
    {
        std::regex pattern(R"(v1/files/([^/]+)/content)");
        std::smatch match;
        std::string url(p + 1);
        if (m_response_str)
        {
            delete[] m_response_str;
        }
        if (std::regex_search(url, match, pattern))
        {
            if (match.size() > 1)
            {
                std::string task_id = match[1].str();
                getparseresult_connectionRAII getparselcon(&getparseresultconn, m_getparseresult_pool);
                nlohmann::json json_1 = getparseresultconn->get_parse_result_python_server(task_id);
                std::string json_str = json_1.dump();
                m_response_str = new char[json_str.length() + 1];
                // 复制字符串内容
                std::strcpy(m_response_str, json_str.c_str());
            }
            else
            {
                // 分配新的内存
                std::string msg = "错误的请求";
                m_response_str = new char[msg.length() + 1];
                // 复制字符串内容
                std::strcpy(m_response_str, msg.c_str());
            }
        }
        else
        {
            std::string msg = "错误的请求";
            m_response_str = new char[msg.length() + 1];
        }
        return JSON_REQUEST;
    }

    else if (*(p + 1) == '5')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/picture.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '6')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/video.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else if (*(p + 1) == '7')
    {
        char *m_url_real = (char *)malloc(sizeof(char) * 200);
        strcpy(m_url_real, "/fans.html");
        strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

        free(m_url_real);
    }
    else
        strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);
    if (stat(m_real_file, &m_file_stat) < 0)
        return NO_RESOURCE;
    if (!(m_file_stat.st_mode & S_IROTH))
        return FORBIDDEN_REQUEST;
    if (S_ISDIR(m_file_stat.st_mode))
        return BAD_REQUEST;
    int fd = open(m_real_file, O_RDONLY);
    m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}
void http_conn::unmap()
{
    if (m_file_address)
    {
        munmap(m_file_address, m_file_stat.st_size);
        m_file_address = 0;
    }
}

bool http_conn::write()
{
    int temp = 0;

    if (bytes_to_send == 0)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        init();
        return true;
    }

    while (1)
    {
        temp = writev(m_sockfd, m_iv, m_iv_count);

        if (temp < 0)
        {
            if (errno == EAGAIN)
            {
                modfd(m_epollfd, m_sockfd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        bytes_have_send += temp;
        bytes_to_send -= temp;
        if (bytes_have_send >= m_iv[0].iov_len)
        {
            m_iv[0].iov_len = 0;
            m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
            m_iv[1].iov_len = bytes_to_send;
        }
        else
        {
            m_iv[0].iov_base = m_write_buf + bytes_have_send;
            m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
        }

        if (bytes_to_send <= 0)
        {
            unmap();
            modfd(m_epollfd, m_sockfd, EPOLLIN);

            if (m_linger)
            {
                init();
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

bool http_conn::add_response(const char *format, ...)
{
    if (m_write_idx >= WRITE_BUFFER_SIZE)
        return false;
    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
    if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
    {
        va_end(arg_list);
        return false;
    }
    m_write_idx += len;
    va_end(arg_list);
    // LOG_INFO("request:%s", m_write_buf);
    // Log::get_instance()->flush();
    if (strlen(m_write_buf) > 1024)
    {
        char truncatedText[1025]; // 创建一个能容纳1024个字符加字符串结束符的数组
        // 复制前1021个字符到新数组
        std::strncpy(truncatedText, m_write_buf, 1021);
        // 添加 ...
        std::strcpy(truncatedText + 1021, "...");
        truncatedText[1024] = '\0'; // 手动添加字符串结束符
        LOG_INFO("%s", truncatedText);
        Log::get_instance()->flush();
    }
    else
    {
        LOG_INFO("%s", m_write_buf);
        Log::get_instance()->flush();
    }
    return true;
}
bool http_conn::add_status_line(int status, const char *title)
{
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}
bool http_conn::add_headers(int content_len, char *content_type)
{
    add_content_length(content_len);
    add_content_type(content_type);
    add_linger();
    add_blank_line();
}
bool http_conn::add_content_length(int content_len)
{
    return add_response("Content-Length:%d\r\n", content_len);
}
bool http_conn::add_content_type(char *type1)
{
    // return add_response("Content-Type:%s\r\n", "application/json");
    return add_response("Content-Type:%s\r\n", type1);
}
bool http_conn::add_linger()
{
    return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
}
bool http_conn::add_blank_line()
{
    return add_response("%s", "\r\n");
}
bool http_conn::add_content(const char *content)
{
    return add_response("%s", content);
}
bool http_conn::process_write(HTTP_CODE ret)
{
    switch (ret)
    {
    case INTERNAL_ERROR:
    {
        add_status_line(500, error_500_title);
        add_headers(strlen(error_500_form), "text/html");
        if (!add_content(error_500_form))
            return false;
        break;
    }
    case BAD_REQUEST:
    {
        add_status_line(404, error_404_title);
        add_headers(strlen(error_404_form), "text/html");
        if (!add_content(error_404_form))
            return false;
        break;
    }
    case FORBIDDEN_REQUEST:
    {
        add_status_line(403, error_403_title);
        add_headers(strlen(error_403_form), "text/html");
        if (!add_content(error_403_form))
            return false;
        break;
    }
    case UNAUTHORIZED_ERROR:
    {
        add_status_line(403, error_401_title);
        add_headers(strlen(error_401_form), "text/html");
        if (!add_content(error_401_form))
            return false;
        break;
    }
    case TOO_MANY_REQUESTS_ERROR:
    {
        add_status_line(403, error_429_title);
        add_headers(strlen(error_429_form), "text/html");
        if (!add_content(error_429_form))
            return false;
        break;
    }
    case UNPROCESSABLE_ENTITY_ERROR:
    {
        add_status_line(403, error_422_title);
        add_headers(strlen(error_422_form), "text/html");
        if (!add_content(error_422_form))
            return false;
        break;
    }
    case PAYLOAD_TOO_LARGE_ERROR:
    {
        add_status_line(413, error_413_title);
        add_headers(strlen(error_413_form), "application/json");
        if (!add_content(error_413_form))
            return false;
        break;
    }
    case FILE_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (m_file_stat.st_size != 0)
        {
            add_headers(m_file_stat.st_size, "text/html");
            m_iv[0].iov_base = m_write_buf;
            m_iv[0].iov_len = m_write_idx;
            m_iv[1].iov_base = m_file_address;
            m_iv[1].iov_len = m_file_stat.st_size;
            m_iv_count = 2;
            bytes_to_send = m_write_idx + m_file_stat.st_size;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string), "text/html");
            if (!add_content(ok_string))
                return false;
        }
        break;
    }
    case JSON_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (strlen(m_response_str) != 0)
        {
            add_headers(strlen(m_response_str), "applocation/json");
            add_content(m_response_str);
            break;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string), "text/html");
            if (!add_content(ok_string))
                return false;
        }
        break;
    }
    case STRING_REQUEST:
    {
        add_status_line(200, ok_200_title);
        if (strlen(m_response_str) != 0)
        {
            add_headers(strlen(m_response_str), "text/html;charset=utf-8");
            add_content(m_response_str);
            break;
            return true;
        }
        else
        {
            const char *ok_string = "<html><body></body></html>";
            add_headers(strlen(ok_string), "text/html");
            if (!add_content(ok_string))
                return false;
        }
        break;
    }
    default:
        return false;
    }
    m_iv[0].iov_base = m_write_buf;
    m_iv[0].iov_len = m_write_idx;
    m_iv_count = 1;
    bytes_to_send = m_write_idx;
    return true;
}
void http_conn::process()
{
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST)
    {
        modfd(m_epollfd, m_sockfd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret)
    {
        close_conn();
    }
    modfd(m_epollfd, m_sockfd, EPOLLOUT);
}
