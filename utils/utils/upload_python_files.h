#ifndef _UPLOAD_PYTHON_FILES_
#define _UPLOAD_PYTHON_FILES_

#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include "../nolhmann/json.h"

// 生成表单数据
template<typename T = void>
std::string uploadfile_buildFormData(const std::map<std::string, std::string>& formDataMap, const std::string& boundary) {
    std::string formData;
    const std::string& contentType = formDataMap.at("fileContentType");
    const std::string& fileContent = formDataMap.at("fileContent");
    for (const auto& pair : formDataMap) {
        if (pair.first == "filename" && contentType.size()>0 && fileContent.size()>0) {
            // 处理文件字段
            formData += "--" + boundary + "\r\n";
            formData += "Content-Disposition: form-data; name=\"" + std::string("file") + "\"; filename=\"" + pair.second + "\"\r\n";
            formData += "Content-Type: " + contentType + "\r\n";
            formData += "\r\n";
            formData += fileContent;
            formData += "\r\n";
        } else {
            // 处理普通字段
            formData += "--" + boundary + "\r\n";
            formData += "Content-Disposition: form-data; name=\"" + pair.first + "\"\r\n";
            formData += "\r\n";
            formData += pair.second + "\r\n";
        }
    }
    formData += "--" + boundary + "--\r\n";
    return formData;
}

// 解析 JSON 数据
template<typename T = void>
nlohmann::json uploadfile_get_josn_body(std::string &all){
    nlohmann::json json_body;
    // 分割出body部分
    size_t pos1 = all.find("\r\n\r\n");
    if (pos1 != std::string::npos) {
        std::string body = all.substr(pos1 + 4);
        // 解析字符串为json
        json_body = nlohmann::json::parse(body);
    }
    return json_body;
}

// 发送 HTTPS 表单请求
template<typename T = void>
nlohmann::json uploadfile_sendHttpsFormRequest(const std::string& serverUrl, const std::string& serverIp, int serverPort, const std::map<std::string, std::string>& formDataMap) {
    const std::string boundary = "e66d2728ed9ed8f090ddee83441f6307";

    // 初始化 OpenSSL
    SSL_library_init();
    OpenSSL_add_all_algorithms();
    SSL_load_error_strings();
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
    }

    // 创建套接字
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        std::cerr << "创建套接字失败" << std::endl;
        SSL_CTX_free(ctx);
    }

    struct timeval send_timeout;
    struct timeval recv_timeout;
    // 设置接收超时时间
    send_timeout.tv_sec = 30;
    send_timeout.tv_usec = 0;
    recv_timeout.tv_sec = 5;
    recv_timeout.tv_usec = 0;
    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) < 0) {
        perror("setsockopt SO_RCVTIMEO failed");
        close(sockfd);
    }

    // 设置发送超时时间
    if (setsockopt(sockfd, SOL_SOCKET, SO_SNDTIMEO, &send_timeout, sizeof(send_timeout)) < 0) {
        perror("setsockopt SO_SNDTIMEO failed");
        close(sockfd);
    }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    std::string _port = std::to_string(serverPort);
    int status = getaddrinfo(serverIp.c_str(), _port.c_str(), &hints, &res);

    // 连接服务器
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "连接服务器失败" << std::endl;
        close(sockfd);
        SSL_CTX_free(ctx);
    }

    // 创建 SSL 对象
    SSL* ssl = SSL_new(ctx);
    SSL_set_fd(ssl, sockfd);

    // 建立 SSL 连接
    if (SSL_connect(ssl) != 1) {
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
    }

    // 构建表单数据
    std::string formData = uploadfile_buildFormData(formDataMap, boundary);

    // 构建 HTTP 请求头
    std::string request = "POST " + serverUrl + " HTTP/1.1\r\n";
    request += "Host: " + serverIp + ":" + std::to_string(serverPort) + "\r\n";
    request += "Content-Type: multipart/form-data; boundary=" + boundary + "\r\n";
    request += "Content-Length: " + std::to_string(formData.length()) + "\r\n";
    request += "Authorization: Bearer sk-eUWdDGt2soL7aIpFRitTy62V9xIANSP6pP1Pf6v50Y190Ul4\r\n";
    request += "Accept: application/json\r\n";
    request += "User-Agent: OpenAI/Python 1.55.3\r\n";
    request += "\r\n";
    request += formData;

    // 发送请求
    if (SSL_write(ssl, request.c_str(), request.length()) <= 0) {
        ERR_print_errors_fp(stderr);
        SSL_shutdown(ssl);
        SSL_free(ssl);
        close(sockfd);
        SSL_CTX_free(ctx);
    }

    // 接收响应
    char buffer[1024];
    int bytesRead;
    std::string aa;
    while (SSL_get_shutdown(ssl)!=SSL_RECEIVED_SHUTDOWN &&
     ((bytesRead = SSL_read(ssl, buffer, sizeof(buffer) - 1)) > 0)) {
        buffer[bytesRead] = '\0';
        aa.append(buffer, bytesRead);
        memset(buffer, '\0', 1024);
    }
    nlohmann::json json_body = uploadfile_get_josn_body(aa);
    if(json_body == NULL){
        std::cout << "上传失败" <<std::endl;
    }

    // 关闭 SSL 连接和套接字
    SSL_shutdown(ssl);
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);
    return json_body;
}

// 上传文件到 Python 服务器
template<typename T = void>
nlohmann::json upload_file_to_pythonsever(const std::string &task_id, const std::string &filename, const std::string &fileContent, const std::string &fileContentType) {
    std::map<std::string, std::string> formDataMap = {
        {"task_id", task_id},
        {"filename", filename},
        {"fileContent", fileContent},
        {"fileContentType", fileContentType},
    };
    std::string serverUrl = "/upload_file";
    std::string serverIp = "127.0.0.1";
    int serverPort = 8000;
    return uploadfile_sendHttpsFormRequest(serverUrl, serverIp, serverPort, formDataMap);
}

#endif