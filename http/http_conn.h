#pragma once

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <sys/stat.h>
#include <string.h>
#include <string>
#include <vector>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/uio.h>
#include <hiredis/hiredis.h>
#include "../lock/locker.h"
#include "../CGImysql/sql_connection_pool.h"
#include "../CGIredis/redis_connection_pool.h"
#include "../CGIrabbitmq/rabbitmq_connection_pool.h"
#include "../CGIuploadfile/uploadfile_connection_pool.h"
#include "../CGIgetparseresult/getparseresult_connection_pool.h"
#include "../token/tokenpool.h"

class http_conn
{
public:
    static const int FILENAME_LEN = 200;
    // static const int READ_BUFFER_SIZE = 2048;
    static const int READ_BUFFER_SIZE = 1024 * 1024 * 15; //必须大于等于 MAX_UPLOADFILE_SIZE 不然无法全部接收
    // static const int WRITE_BUFFER_SIZE = 1024;
    static const int WRITE_BUFFER_SIZE = 1024 * 1024;
    static const int MAX_UPLOADFILE_SIZE = 1024 * 1024 * 10; // 10M
    enum METHOD
    {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    enum CHECK_STATE
    {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    enum HTTP_CODE
    {
        NO_REQUEST,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        JSON_REQUEST,
        STRING_REQUEST,
        INTERNAL_ERROR,
        UNAUTHORIZED_ERROR,
        TOO_MANY_REQUESTS_ERROR,
        UNPROCESSABLE_ENTITY_ERROR,
        PAYLOAD_TOO_LARGE_ERROR,
        CLOSED_CONNECTION
    };
    enum LINE_STATUS
    {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };

public:
    http_conn() {
        m_write_buf = new char[WRITE_BUFFER_SIZE];
        m_read_buf = new char[READ_BUFFER_SIZE];
    }
    ~http_conn() {
        // std::cout << "销毁http" <<std::endl;
        if(m_write_buf){
            delete[] m_write_buf;
        }
        if(m_read_buf){
            delete[] m_read_buf;
        }
    }

public:
    void init(int sockfd, const sockaddr_in &addr);
    void close_conn(bool real_close = true);
    void process();
    bool read_once();
    bool write();
    sockaddr_in *get_address()
    {
        return &m_address;
    }
    void initmysql_result(connection_pool *connPool);
    void initredis_result(redis_connection_pool *connPool);

private:
    void init();
    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_headers(char *text);
    HTTP_CODE parse_content(char *text);
    HTTP_CODE do_request();
    char *get_line() { return m_read_buf + m_start_line; };
    LINE_STATUS parse_line();
    void unmap();
    bool add_response(const char *format, ...);
    bool add_content(const char *content);
    bool add_status_line(int status, const char *title);
    bool add_headers(int content_length, char* type1);
    bool add_content_type(char* type1);
    bool add_content_length(int content_length);
    bool add_linger();
    bool add_blank_line();

public:
    static int m_epollfd;
    static int m_user_count;
    MYSQL *mysql;
    connection_pool *m_mysql_pool;
    // char *m_token;
    vector<string> m_token;
    token_pool *m_token_pool;
    redisContext *redis;
    redis_connection_pool *m_redis_pool;
    AmqpClient::Channel::ptr_t rabbitmq;
    rabbitmq_connection_pool *m_rabbitmq_pool;
    uploadFileConn* uploadfileconn;
    uploadfile_connection_pool *m_uploadfileconn_pool;
    getParseResultConn* getparseresultconn;
    getparseresult_connection_pool *m_getparseresult_pool;
    int m_apikey_max = 10; // 最大apikey并发

private:
    int m_sockfd;
    sockaddr_in m_address;
    // char m_read_buf[READ_BUFFER_SIZE];
    char* m_read_buf;
    int m_read_idx;
    int m_checked_idx;
    int m_start_line;
    // char m_write_buf[WRITE_BUFFER_SIZE];
    char* m_write_buf;
    int m_write_idx;
    CHECK_STATE m_check_state;
    METHOD m_method;
    char m_real_file[FILENAME_LEN];
    char *m_response_str; 
    char *m_url;
    char *m_version;
    char *m_host;
    char *m_content_type;
    char *m_form_data_boundary;
    int m_content_length;
    bool m_linger;
    char *m_file_address;
    char *m_authorization;
    char *m_apikey;
    struct stat m_file_stat;
    struct iovec m_iv[2];
    int m_iv_count;
    int cgi;        //是否启用的POST
    char *m_string; //存储请求头数据
    int bytes_to_send;
    int bytes_have_send;
};

