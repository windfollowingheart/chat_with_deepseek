#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string>
#include "../nolhmann/json.h"

// 移除字符串中的反斜杠
std::string removeBackslashes(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c != '\\') {
            result += c;
        }
    }
    return result;
}

// 发送 POST 请求并携带 JSON 请求体
bool sendEmail(std::string email) {
    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return false;
    }

    // 设置服务器地址
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(8000); // 服务器端口
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    // 连接到服务器
    if (connect(sock, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) == -1) {
        std::cerr << "Failed to connect to server" << std::endl;
        close(sock);
        return false;
    }

    // 定义 JSON 请求体
    // std::string jsonBody = "{\"email\": \"2916311184@qq.com\"}";
    nlohmann::json json1;
    json1["email"] = email;
    std::string jsonBody = json1.dump();

    // 构建 HTTP POST 请求
    std::string request = "POST /email HTTP/1.1\r\n";
    request += "Host: 127.0.0.1:8000\r\n";
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(jsonBody.length()) + "\r\n";
    request += "Connection: close\r\n\r\n";
    request += jsonBody;

    // 发送请求
    if (send(sock, request.c_str(), request.length(), 0) == -1) {
        std::cerr << "Failed to send request" << std::endl;
        close(sock);
        return false;
    }

    // 接收响应
    char buffer[1024];
    ssize_t bytesRead;
    std::string response;
    while ((bytesRead = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response.append(buffer, bytesRead);
    }
    // 解析出响应体
    std::string responseBody;
    size_t bodyStart = response.find("\r\n\r\n");
    if (bodyStart != std::string::npos) {
        responseBody = response.substr(bodyStart + 4);
    }

    // 输出响应
    std::cout << "Response from server:" << std::endl;
    std::cout << responseBody << std::endl;
    size_t end1 = responseBody.size() - 2;
    std::string responseBody1 = responseBody.substr(1, end1);
    std::cout << responseBody1 << std::endl;
    nlohmann::json json11 = nlohmann::json::parse(removeBackslashes(responseBody1));
    // nlohmann::json json11 = nlohmann::json::parse(responseBody);
    std::cout << json11 << std::endl;
    // std::cout << typeid(json11).name() << std::endl;
    std::cout << json11["isok"] << std::endl;
    bool isok = json11["isok"].get<bool>();

    // 关闭套接字
    close(sock);
    return isok;
}

