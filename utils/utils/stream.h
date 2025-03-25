#include <iostream>
#include <string>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include "../nolhmann/json.h"
#include "./sign.h"
#include <vector>
#include <sstream>
#include <netdb.h>
#include <regex>



// 模拟 get_body 函数
nlohmann::json get_body(const std::string& query, const std::string& user_id, const std::string& conversation_id, bool is_new_conversation, int turn_index, bool use_think, bool use_search, const std::vector<std::string>& file_arr = {}) {
    // 定义 use_think_body
    nlohmann::json use_think_body = {
        {"icon", "https://wy-static.wenxiaobai.com/tuwen_image/3f3fdf1cba4ab50ead55571f9d05ae4b0eb2a6df9401edede644a2bc66fc6698"},
        {"title", "深度思考(R1)"},
        {"defaultQuery", ""},
        {"capability", "otherBot"},
        {"capabilityRang", 0},
        {"minAppVersion", ""},
        {"botId", 200004},
        {"botDesc", "深度回答这个问题（Deepseek R1）"},
        {"selectedIcon", "https://wy-static.wenxiaobai.com/tuwen_image/e619ae7984a65e5645cce5db7864670b4b88748d9240664ab4b97cf217c2a4d3"},
        {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
        {"exclusiveCapabilities", nullptr},
        {"defaultSelected", true},
        {"defaultHidden", false},
        {"_id", 0}
    };

    // 定义 use_search_body
    nlohmann::json use_search_body = {
        {"icon", "https://wy-static.wenxiaobai.com/tuwen_image/b22470c0aed13bb629878dc1520ea27d2973a83b15ad66f17069e51a484795e6"},
        {"title", "联网搜索"},
        {"defaultQuery", ""},
        {"capability", "otherBot"},
        {"capabilityRang", 0},
        {"minAppVersion", ""},
        {"botId", 200007},
        {"botDesc", "深度回答这个问题（Deepseek R1）"},
        {"selectedIcon", "https://wy-static.wenxiaobai.com/tuwen_image/447eebdd4af05d065100ed2d59d26167b6162a14607bbdf50f97907e6e9b1cee"},
        {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
        {"exclusiveCapabilities", {"file", "camera", "image"}},
        {"defaultSelected", true},
        {"defaultHidden", false},
        {"_id", 1}
    };

    // 定义 capabilities 数组
    std::vector<nlohmann::json> capabilities;
    if (use_think) {
        capabilities.push_back(use_think_body);
    }
    if (use_search) {
        capabilities.push_back(use_search_body);
    }

    // 构建最终的 body JSON 对象
    nlohmann::json body = {
        {"userId", user_id},
        {"botId", "200006"},
        {"botAlias", "custom"},
        {"query", query},
        {"isRetry", false},
        {"breakingStrategy", 0},
        {"isNewConversation", is_new_conversation},
        {"mediaInfos", file_arr},
        {"turnIndex", turn_index},
        {"rewriteQuery", ""},
        {"conversationId", conversation_id},
        {"capabilities", capabilities},
        {"attachmentInfo", {
            {"url", {
                {"infoList", nlohmann::json::array()}
            }}
        }},
        {"inputWay", "proactive"}
    };

    return body;
}




std::string aaa;
std::string event_type;
std::string model_name = "deepseek-v3";
// 解析 data: 字段
std::string parseData(const std::string& lines) {
    nlohmann::json data_json;
    std::string data_str = "";

    size_t pos_event = lines.find("event:");
    size_t pos_data = lines.find("data:");
    if (pos_event != std::string::npos) {
        std::string eventPart = lines.substr(pos_event + 6);

        size_t first = eventPart.find("\n");
        if (first != std::string::npos) {
            eventPart = eventPart.substr(0, first);
        }
        event_type = eventPart;
        // std::cout << "Parsed event: " << eventPart << eventPart.length() << first << std::endl;
        // std::cout << "Parsed event: " << event_type << std::endl;
    }
    if (pos_data != std::string::npos) {
        std::string dataPart = lines.substr(pos_data + 5);
        

        // 去除尾\n
        size_t last = dataPart.find_last_not_of("\n");
        if (last != std::string::npos) {
            dataPart = dataPart.substr(0, last+1);
        }
        data_json = nlohmann::json::parse(dataPart);
        if(data_json.contains("content") && data_json["content"].is_string() && event_type=="message"){
            aaa += data_json["content"];
            data_str = data_json["content"];
        }
        // std::cout << "Parsed data: " << dataPart << std::endl;
    }
    
    std::vector<nlohmann::json> choices;
    nlohmann::json delta = {
        {"role", "assistant"},
        {"content", data_str}
    };
    choices.push_back(delta);
    nlohmann::json data = {
        {"choices", choices},
        {"model", model_name}
    };
    return data.dump();
}


int chat(std::string query, int connfd) {
    std::string url = "api-bj.wenxiaobai.com";
    // std::string query = "详细介绍鲁迅";
    std::string user_id = "103452339";
    std::string conversation_id = "";
    bool is_new_conversation = true;
    int turn_index = 0;
    bool use_think = true;
    bool use_search = true;
    std::vector<std::string> file_arr;
    std::string token = "eyJ6aXAiOiJHWklQIiwiYWxnIjoiSFM1MTIifQ.H4sIAAAAAAAA_xVOQQ7CMAz7S86blDZts_AO7qjdihgSpaIbYkL8nTQnO47tfOG-rXCC6AMmmWXECWV0ZPIoi1zHPLE1c7KTdwEGWOMGJ8MO9ZzRDND2pO52tC0_ut6a0mOPpd1W5XFfenitivOndi9bDCzdW2_PklVmnyRYSjOZsHC60hytJLL6Ezkx_mKYZFLgyAtfsI_tZZptkJy3RDJAiqVkXeEAe8uv81E1XGtyeWtJfT0X-P0BzAbRYu8AAAA.3UvgBA8_ztRZT-6geuM3IJMUMYmE-2j0-KJu8hV40OlfQJidXISHK9NPoNRadY42fPNFutOKTywvJ3BH1gRing";
    bool stream = false;

    // 创建套接字
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        std::cerr << "Failed to create socket" << std::endl;
        return 1;
    }

     // 设置套接字接收超时时间为 5 秒
     struct timeval timeout;
     timeout.tv_sec = 5;
     timeout.tv_usec = 0;
     if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0) {
         std::cerr << "Failed to set receive timeout" << std::endl;
         close(sock);
         return 1;
     }

    struct addrinfo hints, *res;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int status = getaddrinfo(url.c_str(), "443", &hints, &res);
    if (status != 0) {
        std::cerr << "getaddrinfo error: " << gai_strerror(status) << std::endl;
        close(sock);
        return 1;
    }

    // 连接到服务器
    if (connect(sock, res->ai_addr, res->ai_addrlen) == -1) {
        std::cerr << "Connection failed" << std::endl;
        close(sock);
        freeaddrinfo(res);
        return 1;
    }

    // 构建请求体
    nlohmann::json body = get_body(query, user_id, conversation_id, is_new_conversation, turn_index, use_think, use_search, file_arr);
    std::cout << body << std::endl;
    std::string body_str = body.dump();
    std::cout << body_str << std::endl;
    // 构建请求头
    // std::string header = get_header(body_str, token);
    std::string header = jsonToSocketHeader(body_str, token);
    std::cout << header << std::endl;
    // 发送请求
    std::string request =  header + body_str;

    

    // 创建 SSL 上下文
    SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        return 1;
    }

     // 创建 SSL 对象
     SSL *ssl = SSL_new(ctx);
     if (ssl == NULL) {
         ERR_print_errors_fp(stderr);
         return 1;
     }
 
     // 将 SSL 对象与套接字关联
     if (SSL_set_fd(ssl, sock) != 1) {
         ERR_print_errors_fp(stderr);
         return 1;
     }
 
     // 执行 SSL 握手
     if (SSL_connect(ssl) != 1) {
         ERR_print_errors_fp(stderr);
         return 1;
     }
 
     // 发送请求
     if (SSL_write(ssl, request.c_str(), request.length()) <= 0) {
         ERR_print_errors_fp(stderr);
         return 1;
     }

    // if (send(sock, request.c_str(), request.length(), 0) == -1) {
    //     std::cerr << "Send failed" << std::endl;
    //     close(sock);
    //     freeaddrinfo(res);
    //     return 1;
    // }

    std::string response;
    char buffer[1024];
    ssize_t bytes_received;

    if (stream) {
        // 流式响应处理逻辑
        while ((bytes_received = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        // while ((bytes_received = SSL_read(sock, buffer, sizeof(buffer), 0)) > 0) {
            response.append(buffer, bytes_received);
            // 这里可以添加对流式数据的具体处理逻辑
            std::cout << std::string(buffer, bytes_received);
        }
    } else {
        // 接收流式响应
        const int bufferSize = 4096;
        char buffer[bufferSize];
        // std::string line;
        std::string receivedData; // 存储接收到的所有数据
        int bytesRead;
        int a = 0;
        while ((bytesRead = SSL_read(ssl, buffer, bufferSize - 1)) > 0 && event_type!="close") {
            if (bytesRead > 0) {
                
                buffer[bytesRead] = '\0';
                receivedData += buffer;

                std::regex pattern(R"(\r\n.*\r\n)");
                std::string result = std::regex_replace(receivedData, pattern, "");
                receivedData = result;
                size_t pos = receivedData.find("\n\n");
                while (pos != std::string::npos) {
                    std::string completeMessage = receivedData.substr(0, pos);
                    std::string data = parseData(completeMessage);

                    // 移除已处理的消息
                    receivedData = receivedData.substr(pos + 2);
                    pos = receivedData.find("\n\n");
                    // std::cout << a++ <<std::endl;
                }
            } else if (bytesRead == 0  ) {
                // 处理最后一行
                std::cout << "111" << std::endl;
                if (!receivedData.empty()) {
                    parseData(receivedData);
                }
                std::cout << "Connection closed by server." << std::endl;
                break;
            } else {
                // 发生错误
                int error = SSL_get_error(ssl, bytesRead);
                if (error == SSL_ERROR_WANT_READ || error == SSL_ERROR_WANT_WRITE) {
                    // 非阻塞模式下需要等待
                    continue;
                } else {
                    ERR_print_errors_fp(stderr);
                    break;
                }
            }
        }

        // 输出响应
        // std::cout << response << std::endl;
        std::cout << aaa << std::endl;
    }

    if (bytes_received == -1) {
        std::cerr << "Receive failed" << std::endl;
    }

    // 关闭套接字
    // 清理资源
    SSL_shutdown(ssl);
    SSL_free(ssl);
    SSL_CTX_free(ctx);
    close(sock);
    freeaddrinfo(res);
    // close(sock);
    // freeaddrinfo(res);

    return 0;
}