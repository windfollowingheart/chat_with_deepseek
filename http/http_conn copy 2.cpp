// #include "http_conn.h"
// #include "../log/log.h"
// #include <map>
// #include <list>
// #include <mysql/mysql.h>
// #include <hiredis/hiredis.h>
// #include <fstream>
// #include "../utils/utils/apikey.h"
// #include "../utils/utils/file.h"
// #include "../utils/nolhmann/json.h"
// #include "./stream.h"


// //#define connfdET //边缘触发非阻塞
// #define connfdLT //水平触发阻塞

// //#define listenfdET //边缘触发非阻塞
// #define listenfdLT //水平触发阻塞

// //定义http响应的一些状态信息
// const char *ok_200_title = "OK";
// const char *error_400_title = "Bad Request";
// const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
// const char *error_403_title = "Forbidden";
// const char *error_403_form = "You do not have permission to get file form this server.\n";
// const char *error_404_title = "Not Found";
// const char *error_404_form = "The requested file was not found on this server.\n";
// const char *error_500_title = "Internal Error";
// const char *error_500_form = "There was an unusual problem serving the request file.\n";
// const char *error_401_title= "Unauthentication";
// const char *error_401_form = "Invalid authentication credentials.\n"; // HTTP_401_UNAUTHORIZED
// const char *error_429_title= "TOO_MANY_REQUESTS";
// const char *error_429_form = "The API Key is currently busy processing requests. Please try again later.\n"; // HTTP_429_TOO_MANY_REQUESTS
// const char *error_422_title= "TOO_MANY_REQUESTS";
// const char *error_422_form = "Invalid model value. Allowed values are \"deepseek-v3\", \"deepseek-r1\".\n"; // HTTP_422_UNPROCESSABLE_ENTITY

// //当浏览器出现连接重置时，可能是网站根目录出错或http响应格式出错或者访问的文件中内容完全为空
// const char *doc_root = "/home/wqt/projects/cpp/TinyWebServer/root";

// //将表中的用户名和密码放入map
// map<string, string> users;
// vector<string> apikeys;
// locker m_lock;

// void http_conn::initmysql_result(connection_pool *connPool)
// {
//     //先从连接池中取一个连接
//     MYSQL *mysql = NULL;
//     connectionRAII mysqlcon(&mysql, connPool);

//     //在user表中检索username，passwd数据，浏览器端输入
//     // if (mysql_query(mysql, "SELECT username,passwd FROM user"))
//     if (mysql_query(mysql, "SELECT username,passwd,apikey FROM user"))
//     {
//         LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
//     }

//     //从表中检索完整的结果集
//     MYSQL_RES *result = mysql_store_result(mysql);

//     //返回结果集中的列数
//     int num_fields = mysql_num_fields(result);

//     //返回所有字段结构的数组
//     MYSQL_FIELD *fields = mysql_fetch_fields(result);

//     //从结果集中获取下一行，将对应的用户名和密码，存入map中
//     while (MYSQL_ROW row = mysql_fetch_row(result))
//     {
//         string temp1(row[0]);
//         string temp2(row[1]);
//         users[temp1] = temp2;

//         apikeys.push_back(row[2]);
//     }
//     std::cout<<"apikeys: " << apikeys.size() <<std::endl;
// }

// void http_conn::initredis_result(redis_connection_pool *connPool){
//     //先从连接池中取一个连接
//     redisContext *redis = NULL;
//     redis_connectionRAII redislcon(&redis, connPool);
//     // 选择指定的数据库
//     // redisReply* reply = static_cast<redisReply*>(redisCommand(redis, "SELECT %d", 3));
//     // if (reply == nullptr) {
//     //     std::cerr << "执行 SELECT 命令时出错" << std::endl;
//     //     // redisFree(redis);
//     //     return ;
//     // }
//     redisReply* reply;

//     for(int i=0;i<apikeys.size();i++){
//         std::string apikey = apikeys[i];
//         // 执行 Redis 命令
//         reply = static_cast<redisReply*>(redisCommand(redis, "SET %s %d", apikey.c_str(), 0));
//         if (reply == nullptr) {
//             std::cerr << "Error executing command" << std::endl;
//         }
//     }
//     freeReplyObject(reply);
// }

// //对文件描述符设置非阻塞
// int setnonblocking(int fd)
// {
//     int old_option = fcntl(fd, F_GETFL);
//     int new_option = old_option | O_NONBLOCK;
//     fcntl(fd, F_SETFL, new_option);
//     return old_option;
// }

// //将内核事件表注册读事件，ET模式，选择开启EPOLLONESHOT
// void addfd(int epollfd, int fd, bool one_shot)
// {
//     epoll_event event;
//     event.data.fd = fd;

// #ifdef connfdET
//     event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
// #endif

// #ifdef connfdLT
//     event.events = EPOLLIN | EPOLLRDHUP;
// #endif

// #ifdef listenfdET
//     event.events = EPOLLIN | EPOLLET | EPOLLRDHUP;
// #endif

// #ifdef listenfdLT
//     event.events = EPOLLIN | EPOLLRDHUP;
// #endif

//     if (one_shot)
//         event.events |= EPOLLONESHOT;
//     epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
//     setnonblocking(fd);
// }

// //从内核时间表删除描述符
// void removefd(int epollfd, int fd)
// {
//     epoll_ctl(epollfd, EPOLL_CTL_DEL, fd, 0);
//     close(fd);
// }

// //将事件重置为EPOLLONESHOT
// void modfd(int epollfd, int fd, int ev)
// {
//     epoll_event event;
//     event.data.fd = fd;

// #ifdef connfdET
//     event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
// #endif

// #ifdef connfdLT
//     event.events = ev | EPOLLONESHOT | EPOLLRDHUP;
// #endif

//     epoll_ctl(epollfd, EPOLL_CTL_MOD, fd, &event);
// }

// int http_conn::m_user_count = 0;
// int http_conn::m_epollfd = -1;

// //关闭连接，关闭一个连接，客户总量减一
// void http_conn::close_conn(bool real_close)
// {
//     if (real_close && (m_sockfd != -1))
//     {
//         removefd(m_epollfd, m_sockfd);
//         m_sockfd = -1;
//         m_user_count--;
//     }
// }

// //初始化连接,外部调用初始化套接字地址
// void http_conn::init(int sockfd, const sockaddr_in &addr)
// {
//     m_sockfd = sockfd;
//     m_address = addr;
//     //int reuse=1;
//     //setsockopt(m_sockfd,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse));
//     addfd(m_epollfd, sockfd, true);
//     m_user_count++;
//     init();
// }

// //初始化新接受的连接
// //check_state默认为分析请求行状态
// void http_conn::init() 
// {
//     std::cerr << "初始化了" <<std::endl;
//     // if(m_apikey){
//     //     std::cerr << m_apikey<< "ooo" <<std::endl;
//     // }else{
//     //     std::cerr << "no m_apikey" <<std::endl;
//     // }
//     mysql = NULL;
//     // redis = NULL;
//     bytes_to_send = 0;
//     bytes_have_send = 0;
//     m_check_state = CHECK_STATE_REQUESTLINE;
//     m_linger = false;
//     m_method = GET;
//     m_url = 0;
//     m_version = 0; 
//     m_content_length = 0;
//     m_host = 0;
//     // m_content_type = 0;
//     // m_form_data_boundary = 0;
//     if(!m_authorization){
//         m_authorization=0;
//     };
//     if(!m_apikey){
//         m_apikey=0;
//     };
//     // m_authorization = 0;
//     // m_apikey = 0;
//     m_start_line = 0;
//     m_checked_idx = 0;
//     m_read_idx = 0;
//     m_write_idx = 0;
//     cgi = 0; 
//     // memset(m_read_buf, '\0', READ_BUFFER_SIZE);
//     memset(m_write_buf, '\0', WRITE_BUFFER_SIZE);
//     memset(m_real_file, '\0', FILENAME_LEN);

//     // 分配一个堆内存给m_read_buf
//     // if(m_read_buf){
//     //     std::cout << "要删除" <<std::endl;
//     //     delete[] (m_read_buf);
//     // }
//     if(!m_read_buf){
//         m_read_buf = new char[READ_BUFFER_SIZE];
//     }
// }

// //从状态机，用于分析出一行内容
// //返回值为行的读取状态，有LINE_OK,LINE_BAD,LINE_OPEN
// http_conn::LINE_STATUS http_conn::parse_line()
// {
//     if(m_content_type 
//         && m_content_length>2048 
//         && (strncasecmp(m_content_type, "multipart/form-data", 19)==0) 
//         && m_check_state == CHECK_STATE_CONTENT 
//         // && !(m_read_idx==m_checked_idx) 
//     ){
//         // char temp;
//         // for (; m_checked_idx < m_read_idx; ++m_checked_idx)
//         // {
//         //     // if(m_content_length>2048 && m_content_type=="multipart/form-data" && m_check_state == CHECK_STATE_CONTENT){
                
//         //     // }
//         //     temp = m_read_buf[m_checked_idx];
//         // }
//         // 开始读取大文件
//         // std::cout << "开始手机" << std::endl;
//         // for (; m_checked_idx < m_read_idx; ++m_checked_idx){
//         //     std::cout << m_checked_idx << std::endl;
//         // }
//         // m_checked_idx = m_read_idx;
//         // std::cout << "长度1: " << m_read_idx <<std::endl;
//         // std::cout << "长度1: " << m_checked_idx <<std::endl;
//         // std::cout << "长度: " << strlen(findLastNull(m_read_buf, m_checked_idx)) <<std::endl;
//         // if(strlen(findLastNull(m_read_buf, m_checked_idx)) == m_content_length){
//         //     return LINE_OPEN;
//         // }
        
        
//         return LINE_OPEN;
//     }else{
//         char temp;
//         for (; m_checked_idx < m_read_idx; ++m_checked_idx)
//         {
//             // if(m_content_length>2048 && m_content_type=="multipart/form-data" && m_check_state == CHECK_STATE_CONTENT){
                
//             // }
//             temp = m_read_buf[m_checked_idx];
//             // std::cout << temp << std::endl;
//             // std::cout << (m_read_buf[m_checked_idx + 1]=='\n') << std::endl;
//             // std::cout << (m_read_buf[m_checked_idx + 1]=='\r') << std::endl;
//             if (temp == '\r')
//             {
//                 if ((m_checked_idx + 1) == m_read_idx){
//                     std::cout << "open" << std::endl;
//                     return LINE_OPEN;
//                 }
//                 else if (m_read_buf[m_checked_idx + 1] == '\n')
//                 {
//                     m_read_buf[m_checked_idx++] = '\0';
//                     m_read_buf[m_checked_idx++] = '\0';
//                     // std::cout << "ok2" << std::endl;
//                     return LINE_OK;
//                 }
//                 // else if (m_read_buf[m_checked_idx + 1] == '\r')
//                 // {
//                 //     m_read_buf[m_checked_idx++] = '\0';
//                 //     m_read_buf[m_checked_idx++] = '\0';
//                 //     std::cout << "ok3" << std::endl;
//                 //     return LINE_OK;
//                 // }
//                 // std::cout << temp << std::endl;
//                 // std::cout << "ASCII: " << static_cast<int>(temp) << std::endl;
//                 // std::cout << "ASCII: " << static_cast<int>(m_read_buf[m_checked_idx + 1]) << std::endl;
//                 // std::cout << "bad1" << std::endl;
//                 return LINE_BAD;
//             }
//             else if (temp == '\n')
//             {
//                 if (m_checked_idx > 1 && m_read_buf[m_checked_idx - 1] == '\r')
//                 {
//                     m_read_buf[m_checked_idx - 1] = '\0';
//                     m_read_buf[m_checked_idx++] = '\0';
//                     return LINE_OK;
//                 }
//                 // std::cout << "bad2" << std::endl;
//                 return LINE_BAD;
//             }
//         }
//     }
//     // std::cout << "open1" << std::endl;
//     return LINE_OPEN;
//     // return LINE_OK;
// }

// //循环读取客户数据，直到无数据可读或对方关闭连接
// //非阻塞ET工作模式下，需要一次性将数据读完
// bool http_conn::read_once()
// {
//     if (m_read_idx >= READ_BUFFER_SIZE)
//     {
//         return false;
//     }
//     int bytes_read = 0;

// #ifdef connfdLT

//     std::cout <<"剩余空间" << READ_BUFFER_SIZE - m_read_idx << std::endl;
//     // if(m_content_length>2048 && m_content_type=="multipart/form-data" && m_check_state == CHECK_STATE_CONTENT){

//     // }else
//     bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);

//     if (bytes_read <= 0)
//     {
//         return false;
//     }

//     m_read_idx += bytes_read;
//     std::cout <<m_read_idx << std::endl;
//     return true;

// #endif

// #ifdef connfdET
//     while (true)
//     {
//         bytes_read = recv(m_sockfd, m_read_buf + m_read_idx, READ_BUFFER_SIZE - m_read_idx, 0);
//         if (bytes_read == -1)
//         {
//             if (errno == EAGAIN || errno == EWOULDBLOCK)
//                 break;
//             return false;
//         }
//         else if (bytes_read == 0)
//         {
//             return false;
//         }
//         m_read_idx += bytes_read;
//     }
//     return true;
// #endif
// }

// //解析http请求行，获得请求方法，目标url及http版本号
// http_conn::HTTP_CODE http_conn::parse_request_line(char *text)
// {
//     m_url = strpbrk(text, " \t");
//     if (!m_url)
//     {
//         return BAD_REQUEST;
//     }
//     *m_url++ = '\0'; // 分割字符串 m_url的位置是:" / HTTP/1.1"相当于把起始空格值为\0， text由"GET / HTTP/1.1" 变为"GET", m_url变为"/ HTTP/1.1"
//     char *method = text;

//     if (strcasecmp(method, "GET") == 0)
//         m_method = GET;
//     else if (strcasecmp(method, "POST") == 0)
//     {
//         m_method = POST;
//         cgi = 1;
//     }
//     else
//         return BAD_REQUEST;
//     m_url += strspn(m_url, " \t");
//     m_version = strpbrk(m_url, " \t");
//     if (!m_version)
//         return BAD_REQUEST;
//     *m_version++ = '\0';
//     m_version += strspn(m_version, " \t");
//     if (strcasecmp(m_version, "HTTP/1.1") != 0)
//         return BAD_REQUEST;
//     if (strncasecmp(m_url, "http://", 7) == 0)
//     {
//         m_url += 7;
//         m_url = strchr(m_url, '/');
//     }

//     if (strncasecmp(m_url, "https://", 8) == 0)
//     {
//         m_url += 8;
//         m_url = strchr(m_url, '/');
//     }
//     if (!m_url || m_url[0] != '/')
//         return BAD_REQUEST;
//     //当url为/时，显示判断界面
//     if (strlen(m_url) == 1)
//         strcat(m_url, "judge.html");
//     m_check_state = CHECK_STATE_HEADER;
//     return NO_REQUEST;
// }

// //解析http请求的一个头部信息
// http_conn::HTTP_CODE http_conn::parse_headers(char *text)
// {
//     if (text[0] == '\0')
//     {
//         if (m_content_length != 0)
//         {
            
//             m_check_state = CHECK_STATE_CONTENT;
//             return NO_REQUEST;
//         }
//         return GET_REQUEST;
//     }
//     else if (strncasecmp(text, "Connection:", 11) == 0)
//     {
//         text += 11;
//         text += strspn(text, " \t");
//         if (strcasecmp(text, "keep-alive") == 0)
//         {
//             m_linger = true;
//         }
//     }
//     else if (strncasecmp(text, "Content-length:", 15) == 0)
//     {
//         text += 15;
//         text += strspn(text, " \t");
//         m_content_length = atol(text);
//     }
//     else if (strncasecmp(text, "Content-Type:", 13) == 0)
//     {
//         text += 13;
//         text += strspn(text, " \t");
//         m_content_type = text;
//         if((strncasecmp(m_content_type, "multipart/form-data; boundary=", 30) == 0)){
//             m_content_type[19] = '\0';
//             m_form_data_boundary = m_content_type + 30;
//         }
        
//     }
//     else if (strncasecmp(text, "Host:", 5) == 0)
//     {
//         text += 5;
//         text += strspn(text, " \t");
//         m_host = text;
//     }
//     else if (strncasecmp(text, "Authorization:", 14) == 0 || strncasecmp(text, "authorization:", 14) == 0)
//     {
//         // std::cerr << text << std::endl;
//         text += 14;
//         text += strspn(text, " \t");
//         m_authorization = text;
//     }
//     else
//     {
//         //printf("oop!unknow header: %s\n",text);
//         LOG_INFO("oop!unknow header: %s", text);
//         Log::get_instance()->flush();
//     }
//     return NO_REQUEST;
// }

// //判断http请求是否被完整读入
// http_conn::HTTP_CODE http_conn::parse_content(char *text)
// {
//     // std::cout << text << std::endl;
//     // std::cout << strlen(text) << std::endl;
//     // std::cout << m_checked_idx << std::endl;
//     // std::cout << m_read_idx << std::endl;
//     // std::cout << strlen(m_read_buf) << std::endl;
//     // std::cout << "999" << std::endl;
//     // std::cout << m_content_length << std::endl;
//     // std::cout << m_read_idx << std::endl;
//     // std::cout << m_checked_idx << std::endl;
//     if (m_read_idx >= (m_content_length + m_checked_idx) && !(strncasecmp(m_content_type, "multipart/form-data", 19)==0))
//     {
//         // std::cout << "ok" << std::endl;
//         text[m_content_length] = '\0';
//         //POST请求中最后为输入的用户名和密码
//         m_string = text;
//         return GET_REQUEST;
//     }
//     if((strncasecmp(m_content_type, "multipart/form-data", 19)==0) && m_read_idx == m_checked_idx + m_content_length){
//         std::cout << "ok1" << std::endl;
//         std::cout << m_checked_idx << std::endl;
//         // std::string receivedData(findLastNull(m_read_buf, m_read_idx));
//         // std::string receivedData(m_read_buf + m_checked_idx);
//         // // std::cout << "ok2" << std::endl;
//         // std::string boundaryData(m_form_data_boundary);
//         // // std::cout << boundaryData << std::endl;
//         // // std::cout << receivedData << std::endl;
//         // size_t pos = receivedData.find("--" + boundaryData);
//         // std::cout << receivedData.size() << std::endl;
//         // if (pos != std::string::npos) {
//         //     std::string completeMessage = receivedData.substr(pos, receivedData.size()-pos);
//         //     std::strcpy(m_string, completeMessage.c_str()); // 复制字符串内容
//         //     std::cout << m_string << std::endl;
//         //     return GET_REQUEST;
//         // }
//         m_string = m_read_buf + m_checked_idx;
//         return GET_REQUEST;
//     }
    
//     return NO_REQUEST;
// }


// http_conn::HTTP_CODE http_conn::process_read()
// {
//     LINE_STATUS line_status = LINE_OK;
//     HTTP_CODE ret = NO_REQUEST;
//     char *text = 0;

//     while ((m_check_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || ((line_status = parse_line()) == LINE_OK))
//     {
//         text = get_line();
//         m_start_line = m_checked_idx;
//         // std::cout << "h" << std::endl;
//         // LOG_INFO("%s", text);
//         // Log::get_instance()->flush();
//         // std::cout << "h3" << std::endl;
//         switch (m_check_state)
//         {
//         case CHECK_STATE_REQUESTLINE:
//         {
//             ret = parse_request_line(text);
//             if (ret == BAD_REQUEST)
//                 return BAD_REQUEST;
//             break;
//         }
//         case CHECK_STATE_HEADER:
//         {
//             ret = parse_headers(text);
//             if (ret == BAD_REQUEST)
//                 return BAD_REQUEST;
//             else if (ret == GET_REQUEST)
//             {
//                 return do_request();
//             }
//             break;
//         }
//         case CHECK_STATE_CONTENT:
//         {
//             // std::cout << "h1" << std::endl;
//             ret = parse_content(text);
//             if (ret == GET_REQUEST)
//                 return do_request();
//             line_status = LINE_OPEN;
//             break;
//         }
//         default:
//             return INTERNAL_ERROR;
//         }
//     }
//     return NO_REQUEST;
// }

// http_conn::HTTP_CODE http_conn::do_request() 
// {
//     std::cout<<"do" <<std::endl;
//     strcpy(m_real_file, doc_root);
//     int len = strlen(doc_root);
//     // const char *p = strrchr(m_url, '/');
//     const char *p = m_url;
//     //处理cgi
//     // if (cgi == 1 && (*(p + 1) == '2' || *(p + 1) == '3'))
//     if (cgi == 1) 
//     {

        
//         //根据标志判断是登录检测还是注册检测
//         char flag = m_url[1];

//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/");
//         strcat(m_url_real, m_url + 2);
//         strncpy(m_real_file + len, m_url_real, FILENAME_LEN - len - 1);
//         free(m_url_real);

//         char name[100], password[100], apikey[100];
//         if(strncasecmp(p+1, "register", 8) == 0 || strncasecmp(p+1, "login", 5) == 0){
//             //将用户名和密码提取出来
//             //user=123&passwd=123
//             int i;
//             for (i = 5; m_string[i] != '&'; ++i)
//                 name[i - 5] = m_string[i];
//             name[i - 5] = '\0';

//             int j = 0;
//             for (i = i + 10; m_string[i] != '\0'; ++i, ++j)
//                 password[j] = m_string[i];
//             password[j] = '\0';
//         }
        

//         //同步线程登录校验
//         // if (*(p + 1) == '3')
//         if (m_method == POST && strncasecmp(p+1, "register", 8) == 0)
//         {
//             //如果是注册，先检测数据库中是否有重名的
//             //没有重名的，进行增加数据
//             char *sql_insert = (char *)malloc(sizeof(char) * 200);
//             // strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
//             gen_api_key(apikey);
//             strcpy(sql_insert, "INSERT INTO user(username, passwd, apikey) VALUES(");
//             strcat(sql_insert, "'");
//             strcat(sql_insert, name);
//             strcat(sql_insert, "', '");
//             strcat(sql_insert, password);
//             strcat(sql_insert, "', '");
//             strcat(sql_insert, apikey);
//             strcat(sql_insert, "')");

//             if (users.find(name) == users.end())
//             {

//                 m_lock.lock();
//                 int res = mysql_query(mysql, sql_insert);
//                 // users.insert(pair<string, string>(name, password));
//                 m_lock.unlock();

//                 if (!res){
//                     users.insert(pair<string, string>(name, password));
//                     strcpy(m_url, "/log.html");
//                 }
//                 else{
//                     strcpy(m_url, "/registerError.html");
//                 }
//             }
//             else{
//                 strcpy(m_url, "/registerError.html");
//             }
//         }
//         //如果是登录，直接判断
//         //若浏览器端输入的用户名和密码在表中可以查找到，返回1，否则返回0
//         // else if (*(p + 1) == '2')
//         else if (m_method == POST && strncasecmp(p+1, "login", 5) == 0)
//         {
//             if (users.find(name) != users.end() && users[name] == password)
//             {
//                 char *sql_find = (char *)malloc(sizeof(char) * 200);
//                 // strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
//                 strcpy(sql_find, "SELECT apikey FROM user WHERE username = '");
//                 strcat(sql_find, name);
//                 strcat(sql_find, "' AND passwd = '");
//                 strcat(sql_find, password);
//                 strcat(sql_find, "'");

//                 if (mysql_query(mysql, sql_find))
//                 {
//                     LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
//                 }

//                 //从表中检索完整的结果集
//                 MYSQL_RES *result = mysql_store_result(mysql);

//                 //从结果集中获取下一行，将对应的用户名和密码，存入map中
//                 MYSQL_ROW row = mysql_fetch_row(result);
//                 string _apikey(row[0]);
//                 string content = generateAuthorizedMessage(_apikey);
//                 if(m_response_str){
//                     delete[] m_response_str;
//                 }
//                 // 分配新的内存
//                 m_response_str = new char[content.length() + 1];
        
//                 // 复制字符串内容
//                 std::strcpy(m_response_str, content.c_str());
//                     // strcpy(m_url, "/welcome.html");
//                 return STRING_REQUEST;
//             }
//             else
//                 strcpy(m_url, "/logError.html");
//         }
//         else if (m_method == POST && strncasecmp(p+1, "v1/chat/completions", 19) == 0)
//         {
           
//             // 首先验证apikey
//             if(m_authorization){
//                 if(strlen(m_authorization)==0 || !(strncasecmp(m_authorization, "Bearer ", 7) == 0)){
//                     return UNAUTHORIZED_ERROR; 
//                 }else{
//                     m_authorization += 7;
                   
//                     // 深拷贝
//                     m_apikey = strdup(m_authorization);
//                     if (m_apikey == nullptr) {
//                         // 处理内存分配失败
//                     }
                    
//                     m_lock.lock();
                    
//                     redisReply* reply = static_cast<redisReply*>(redisCommand(redis, "GET %s", m_apikey));
                    
//                     if (reply == nullptr) {
//                         std::cerr << "Error executing command" << std::endl;
//                         m_lock.unlock();
//                         return UNAUTHORIZED_ERROR;
//                     }
//                     if (reply->type == REDIS_REPLY_NIL) {
//                         std::cerr << "No this apikey" << std::endl;
//                         m_lock.unlock();
//                         return UNAUTHORIZED_ERROR;
//                     }
//                     if(reply->type == REDIS_REPLY_STRING){
//                         int value = std::stoi(reply->str);
                        
//                         if(value>=m_apikey_max){
//                             m_lock.unlock();
//                             std::cerr << "超过MAX" << std::endl;
//                             return TOO_MANY_REQUESTS_ERROR;
//                         }else{
//                             value++;
//                             reply = static_cast<redisReply*>(redisCommand(redis, "SET %s %d", m_apikey, value));
//                             if (reply == nullptr) {
//                                 std::cerr << "Error executing command" << std::endl;
//                                 m_lock.unlock();
//                                 return UNAUTHORIZED_ERROR;
//                             }
//                         }
//                     }
//                     m_lock.unlock();
                    
//                     // 释放回复对象
//                     freeReplyObject(reply);
//                     // m_lock.unlock();
//                 }
//             }else{
//                 return UNAUTHORIZED_ERROR;
//             }
            
//             // 定义 JSON 响应体字符串
//             nlohmann::json data = nlohmann::json::parse(m_string);
//             std::vector<nlohmann::json> contents = data["messages"].get<std::vector<nlohmann::json>>();
//             std::string model = data["model"].get<std::string>();
//             bool search = false;
//             if(data.contains("search") && data["search"].is_boolean()){
//                 search = data["search"].get<bool>(); 
//             }
//             if(model != "deepseek-v3" && model != "deepseek-r1"){
//                 m_lock.lock();
//                 dcr_apikey(redis, m_apikey);
//                 m_lock.unlock();
//                 free(m_apikey);
//                 return UNPROCESSABLE_ENTITY_ERROR;
//             }
//             std::string content = contents[0]["content"].get<std::string>();
//             m_token = m_token_pool->GetToken();
//             chat(content,model, m_token, m_epollfd, m_sockfd, search);
//             m_token_pool->ReleaseToken(m_token);

            
//             m_lock.lock();
//             dcr_apikey(redis, m_apikey);
//             m_lock.unlock();
//             free(m_apikey);
            
//             return NO_REQUEST;   
//         }
//         else if (m_method == POST && strncasecmp(p+1, "files", 5) == 0){
            
//             std::map<std::string, std::string> result = parseMultipartFormData(m_string, m_form_data_boundary);
//             std::string filename = result["filename"];
//             std::string file = result["file"];
//             std::string content = result["file_content"];
//             std::cout << filename << std::endl;
//             std::cout << content << std::endl;
//             std::cout << content.size() << std::endl;
//             std::cout << result["file_start"] << std::endl;
//             std::cout << result["file_end"] << std::endl;
//             return NO_REQUEST;
//         }

//     }

//     // if (*(p + 1) == '0')
//     if (m_method == GET && strncasecmp(p+1, "register", 8) == 0)
//     {
//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/register.html");
//         strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

//         free(m_url_real);
//     }
//     // else if (*(p + 1) == '1')
//     else if (m_method == GET && strncasecmp(p+1, "login", 5) == 0)
//     {
//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/log.html");
//         strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

//         free(m_url_real);
//     }
//     else if (m_method == GET && strncasecmp(p+1, "v1/models", 9) == 0)
//     {
//         // 定义 JSON 响应体字符串
//         char* m_response_str1 = R"({
//             "data": [
//                 {
//                     "id": "deepseek-v3"
//                 },
//                 {
//                     "id": "deepseek-r1"
//                 }
//             ]
//         }
//         )";
//         // 解析 JSON 字符串
//         nlohmann::json j = nlohmann::json::parse(m_response_str1);

//         // 将 JSON 对象转换回字符串
//         std::string json_str = j.dump();
//         // 动态分配内存
//         // 释放原有内存
        
//         if(m_response_str){
//             delete[] m_response_str;
//         }
//         // 分配新的内存
//         m_response_str = new char[json_str.length() + 1];

//         // 复制字符串内容
//         std::strcpy(m_response_str, json_str.c_str());
//         // 输出转换后的字符串
//         return JSON_REQUEST;
//     }
    
//     else if (*(p + 1) == '5')
//     {
//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/picture.html");
//         strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

//         free(m_url_real);
//     }
//     else if (*(p + 1) == '6')
//     {
//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/video.html");
//         strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

//         free(m_url_real);
//     }
//     else if (*(p + 1) == '7')
//     {
//         char *m_url_real = (char *)malloc(sizeof(char) * 200);
//         strcpy(m_url_real, "/fans.html");
//         strncpy(m_real_file + len, m_url_real, strlen(m_url_real));

//         free(m_url_real);
//     }
//     else
//         strncpy(m_real_file + len, m_url, FILENAME_LEN - len - 1);

//     if (stat(m_real_file, &m_file_stat) < 0)
//         return NO_RESOURCE;
//     if (!(m_file_stat.st_mode & S_IROTH))
//         return FORBIDDEN_REQUEST;
//     if (S_ISDIR(m_file_stat.st_mode))
//         return BAD_REQUEST;
//     int fd = open(m_real_file, O_RDONLY);
//     m_file_address = (char *)mmap(0, m_file_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
//     close(fd);
//     return FILE_REQUEST;
// }
// void http_conn::unmap()
// {
//     if (m_file_address)
//     {
//         munmap(m_file_address, m_file_stat.st_size);
//         m_file_address = 0;
//     }
// }

// bool http_conn::write()
// {
//     int temp = 0;

//     if (bytes_to_send == 0)
//     {
//         modfd(m_epollfd, m_sockfd, EPOLLIN);
//         init();
//         return true;
//     }

//     while (1)
//     {
//         temp = writev(m_sockfd, m_iv, m_iv_count);

//         if (temp < 0)
//         {
//             if (errno == EAGAIN)
//             {
//                 modfd(m_epollfd, m_sockfd, EPOLLOUT);
//                 return true;
//             }
//             unmap();
//             return false;
//         }

//         bytes_have_send += temp;
//         bytes_to_send -= temp;
//         if (bytes_have_send >= m_iv[0].iov_len)
//         {
//             m_iv[0].iov_len = 0;
//             m_iv[1].iov_base = m_file_address + (bytes_have_send - m_write_idx);
//             m_iv[1].iov_len = bytes_to_send;
//         }
//         else
//         {
//             m_iv[0].iov_base = m_write_buf + bytes_have_send;
//             m_iv[0].iov_len = m_iv[0].iov_len - bytes_have_send;
//         }

//         if (bytes_to_send <= 0)
//         {
//             unmap();
//             modfd(m_epollfd, m_sockfd, EPOLLIN);

//             if (m_linger)
//             {
//                 init();
//                 return true;
//             }
//             else
//             {
//                 return false;
//             }
//         }
//     }
// }

// bool http_conn::add_response(const char *format, ...)
// {
//     if (m_write_idx >= WRITE_BUFFER_SIZE)
//         return false;
//     va_list arg_list;
//     va_start(arg_list, format);
//     int len = vsnprintf(m_write_buf + m_write_idx, WRITE_BUFFER_SIZE - 1 - m_write_idx, format, arg_list);
//     if (len >= (WRITE_BUFFER_SIZE - 1 - m_write_idx))
//     {
//         va_end(arg_list);
//         return false;
//     }
//     m_write_idx += len;
//     va_end(arg_list);
//     LOG_INFO("request:%s", m_write_buf);
//     Log::get_instance()->flush();
//     return true;
// }
// bool http_conn::add_status_line(int status, const char *title)
// {
//     return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
// }
// bool http_conn::add_headers(int content_len, char* content_type)
// {
//     add_content_length(content_len);
//     add_content_type(content_type);
//     add_linger();
//     add_blank_line();
// }
// bool http_conn::add_content_length(int content_len)
// {
//     return add_response("Content-Length:%d\r\n", content_len);
// }
// bool http_conn::add_content_type(char* type1)
// {
//     // return add_response("Content-Type:%s\r\n", "application/json");
//     return add_response("Content-Type:%s\r\n", type1);
// }
// bool http_conn::add_linger()
// {
//     return add_response("Connection:%s\r\n", (m_linger == true) ? "keep-alive" : "close");
// }
// bool http_conn::add_blank_line()
// {
//     return add_response("%s", "\r\n");
// }
// bool http_conn::add_content(const char *content)
// {
//     return add_response("%s", content);
// }
// bool http_conn::process_write(HTTP_CODE ret)
// {
//     switch (ret)
//     {
//     case INTERNAL_ERROR:
//     {
//         add_status_line(500, error_500_title);
//         add_headers(strlen(error_500_form), "text/html");
//         if (!add_content(error_500_form))
//             return false;
//         break;
//     }
//     case BAD_REQUEST:
//     {
//         add_status_line(404, error_404_title);
//         add_headers(strlen(error_404_form), "text/html");
//         if (!add_content(error_404_form))
//             return false;
//         break;
//     }
//     case FORBIDDEN_REQUEST:
//     {
//         add_status_line(403, error_403_title);
//         add_headers(strlen(error_403_form), "text/html");
//         if (!add_content(error_403_form))
//             return false;
//         break;
//     }
//     case UNAUTHORIZED_ERROR:
//     {
//         add_status_line(403, error_401_title);
//         add_headers(strlen(error_401_form), "text/html");
//         if (!add_content(error_401_form))
//             return false;
//         break;
//     }
//     case TOO_MANY_REQUESTS_ERROR:
//     {
//         add_status_line(403, error_429_title);
//         add_headers(strlen(error_429_form), "text/html");
//         if (!add_content(error_429_form))
//             return false;
//         break;
//     }
//     case UNPROCESSABLE_ENTITY_ERROR:
//     {
//         add_status_line(403, error_422_title);
//         add_headers(strlen(error_422_form), "text/html");
//         if (!add_content(error_422_form))
//             return false;
//         break;
//     }
//     case FILE_REQUEST:
//     {
//         add_status_line(200, ok_200_title);
//         if (m_file_stat.st_size !=0)
//         {
//             add_headers(m_file_stat.st_size, "text/html");
//             m_iv[0].iov_base = m_write_buf;
//             m_iv[0].iov_len = m_write_idx;
//             m_iv[1].iov_base = m_file_address;
//             m_iv[1].iov_len = m_file_stat.st_size;
//             m_iv_count = 2;
//             bytes_to_send = m_write_idx + m_file_stat.st_size;
//             return true;
//         }
//         else
//         {
//             const char *ok_string = "<html><body></body></html>";
//             add_headers(strlen(ok_string), "text/html");
//             if (!add_content(ok_string))
//                 return false;
//         }
//         break;
//     }
//     case JSON_REQUEST:
//     {
//         add_status_line(200, ok_200_title);
//         if (strlen(m_response_str) != 0)
//         {
//             add_headers(strlen(m_response_str), "applocation/json");
//             add_content(m_response_str);
//             break;
//             return true;
//         }
//         else
//         {
//             const char *ok_string = "<html><body></body></html>";
//             add_headers(strlen(ok_string), "text/html");
//             if (!add_content(ok_string))
//                 return false;
//         }
//         break;
//     }
//     case STRING_REQUEST:
//     {
//         add_status_line(200, ok_200_title);
//         if (strlen(m_response_str) != 0)
//         {
//             add_headers(strlen(m_response_str), "text/html;charset=utf-8");
//             add_content(m_response_str);
//             break;
//             return true;
//         }
//         else
//         {
//             const char *ok_string = "<html><body></body></html>";
//             add_headers(strlen(ok_string), "text/html");
//             if (!add_content(ok_string))
//                 return false;
//         }
//         break;
//     }
//     default:
//         return false;
//     }
//     m_iv[0].iov_base = m_write_buf;
//     m_iv[0].iov_len = m_write_idx;
//     m_iv_count = 1;
//     bytes_to_send = m_write_idx;
//     return true;
// }
// void http_conn::process()
// {
//     HTTP_CODE read_ret = process_read();
//     std::cout<<"end!!"<<std::endl;
//     if (read_ret == NO_REQUEST)
//     {
//         modfd(m_epollfd, m_sockfd, EPOLLIN);
//         return;
//     }
//     bool write_ret = process_write(read_ret);
//     if (!write_ret)
//     {
//         close_conn();
//     }
//     modfd(m_epollfd, m_sockfd, EPOLLOUT);
// }
