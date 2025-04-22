#include <iostream>
#include <string>
#include <cstring>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <vector>
#include <sstream>
#include <netdb.h>
#include <fcntl.h>
#include <regex>
#include <sys/epoll.h>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <codecvt>
#include <locale>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/select.h>

#include "../utils/nolhmann/json.h"
#include "../utils/utils/sign.h"



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
    // nlohmann::json use_think_body = {
    //         {"icon", "https://wy-static.wenxiaobai.com/bot-capability/prod/%E6%B7%B1%E5%BA%A6%E6%80%9D%E8%80%83.png"},
    //         {"title", "深度思考R1"},
    //         {"defaultQuery", ""},
    //         {"capability", "otherBot"},
    //         {"capabilityRang", 0},
    //         {"minAppVersion", ""},
    //         {"botId", 200004},
    //         {"botDesc", "擅长复杂推理与深入分析"},
    //         {"selectedIcon", "https://wy-static.wenxiaobai.com/bot-capability/prod/%E6%B7%B1%E5%BA%A6%E6%80%9D%E8%80%83%E9%80%89%E4%B8%AD.png"},
    //         {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
    //         {"exclusiveCapabilities", nullptr},
    //         {"defaultSelected", true},
    //         {"defaultHidden", false},
    //         {"key", "deepseekR1"},
    //         {"defaultPlaceholder", ""},
    //         {"isPromptMenu", false},
    //         {"subCapabilities", nullptr},
    //         {"promptMenu", false},
    //         {"_is_new_tag", false},
    //         {"web_beta", ""}
        
    // };

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
    // nlohmann::json use_search_body = {
        
    //         {"icon", "https://wy-static.wenxiaobai.com/bot-capability/prod/nonetwork.png"},
    //         {"title", "不联网"},
    //         {"defaultQuery", ""},
    //         {"capability", "otherBot"},
    //         {"capabilityRang", 0},
    //         {"minAppVersion", ""},
    //         {"botId", 0},
    //         {"botDesc", "专注创作流畅输出"},
    //         {"selectedIcon", "https://wy-static.wenxiaobai.com/bot-capability/prod/nonetwork.png"},
    //         {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
    //         {"exclusiveCapabilities", {"file", "camera", "image"}},
    //         {"defaultSelected", false},
    //         {"defaultHidden", false},
    //         {"key", "no_search"},
    //         {"defaultPlaceholder", ""},
    //         {"isPromptMenu", false},
    //         {"subCapabilities", nullptr},
    //         {"promptMenu", false},
    //         {"_is_new_tag", false},
    //         {"web_beta", ""}
        
    // };

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

    // body = {
    //     {"userId", 102741223},
    //     {"botId", "200006"},
    //     {"botAlias", "custom"},
    //     {"query", "你好"},
    //     {"isRetry", false},
    //     {"breakingStrategy", 0},
    //     {"isNewConversation", false},
    //     {"mediaInfos", {}},
    //     {"turnIndex", 1},
    //     {"rewriteQuery", ""},
    //     {"conversationId", "9d7b60d1-fbb9-4f4b-8e9a-55687bb7a466"},
    //     {"attachmentInfo", 
    //         {"url", {"infoList", nullptr}}
    //     },
    //     {"inputWay", "proactive"},
    //     {"capabilities", 
    //       {
    //         {"icon", "https://wy-static.wenxiaobai.com/bot-capability/prod/%E6%B7%B1%E5%BA%A6%E6%80%9D%E8%80%83.png"},
    //         {"title", "深度思考R1"},
    //         {"defaultQuery", ""},
    //         {"capability", "otherBot"},
    //         {"capabilityRang", 0},
    //         {"minAppVersion", ""},
    //         {"botId": 200004},
    //         {"botDesc", "擅长复杂推理与深入分析"},
    //         {"selectedIcon", "https://wy-static.wenxiaobai.com/bot-capability/prod/%E6%B7%B1%E5%BA%A6%E6%80%9D%E8%80%83%E9%80%89%E4%B8%AD.png"},
    //         {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
    //         {"exclusiveCapabilities", {}},
    //         {"defaultSelected", true},
    //         {"defaultHidden", false},
    //         {"key", "deepseekR1"},
    //         {"defaultPlaceholder", ""},
    //         {"isPromptMenu", false},
    //         {"subCapabilities", nullptr},
    //         {"promptMenu", false},
    //         {"_is_new_tag", false},
    //         {"web_beta", ""}
    //       },
    //       {
    //         {"icon", "https://wy-static.wenxiaobai.com/bot-capability/prod/fastsearch.png"},
    //         {"title", "日常搜索"},
    //         {"defaultQuery", ""},
    //         {"capability", "otherBot"},
    //         {"capabilityRang", 0},
    //         {"minAppVersion", ""},
    //         {"botId", 200007},
    //         {"botDesc", "即时获取最新信息"},
    //         {"selectedIcon", "https://wy-static.wenxiaobai.com/bot-capability/prod/fastsearch_active.png"},
    //         {"botIcon", "https://platform-dev-1319140468.cos.ap-nanjing.myqcloud.com/bot/avatar/2025/02/06/612cbff8-51e6-4c6a-8530-cb551bcfda56.webp"},
    //         {"exclusiveCapabilities", {
    //           "file",
    //           "camera",
    //           "image"
    //         }},
    //         {"defaultSelected", true},
    //         {"defaultHidden", false},
    //         {"key", "quick_search"},
    //         {"defaultPlaceholder", ""},
    //         {"isPromptMenu", false},
    //         {"subCapabilities": nullptr},
    //         {"promptMenu", false},
    //         {"_is_new_tag", false},
    //         {"web_beta", ""}
    //       }
    //     },
    //     {"pureQuery", ""}
    // }

    return body;
}



int SocketConnected(int sock)
{
    if (sock <= 0)
        return 0;
    struct tcp_info info;
    int len = sizeof(info);
    if (getsockopt(sock, IPPROTO_TCP, TCP_INFO, &info, (socklen_t *) & len) == -1) {
        perror("getsockopt");
        // 这里可以根据具体情况做更合适的处理，如返回特定错误码
        return 0; 
    }
    if ((info.tcpi_state == TCP_ESTABLISHED)) {
        return 1;
    } 
    else {
        std::cout << info.tcpi_state << std::endl;
        return 0;
    }
}

// 非阻塞模式下检测连接是否存活
bool isConnectionAlive(int socketFd) {
    char buffer[1];
    int result = recv(socketFd, buffer, sizeof(buffer), MSG_PEEK | MSG_DONTWAIT);
    if (result == 0) {
        return false; // 客户端正常关闭
    } else if (result == -1) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true; // 连接仍存活，但无数据
        }
        return false; // 连接异常（如 ECONNRESET）
    }
    return true;
}

void handle_start_client(int client_socket){
    // 发送 HTTP 响应头
    std::string response_header = "HTTP/1.1 200 OK\r\n";
    response_header += "Content-Type: text/event-stream; charset=utf-8\r\n";
    response_header += "Connection: keep-alive\r\n";
    response_header += "Transfer-Encoding: chunked\r\n";
    response_header += "\r\n";
    send(client_socket, response_header.c_str(), response_header.length(), 0);
}


bool handle_client(int epollfd, int client_socket, std::string event_data) {

    // 检测客户端是否断开连接
    // int sent = send(client_socket, "", 0, MSG_NOSIGNAL);
    // if (sent == -1 && (errno == EPIPE || errno == ECONNRESET)) {
    //     std::cerr << "Client disconnected." << std::endl;
    //     return false;
    // }
    if(!SocketConnected(client_socket)){
        std::cerr << "Client disconnected." << std::endl;
        return false;
    }

    // 构造 SSE 格式的数据
    std::string chunk_size_str = "";
    int chunk_size = event_data.length();
    while (chunk_size > 0) {
        int digit = chunk_size % 16;
        if (digit < 10) {
            chunk_size_str = static_cast<char>('0' + digit) + chunk_size_str;
        } else {
            chunk_size_str = static_cast<char>('A' + digit - 10) + chunk_size_str;
        }
        chunk_size /= 16;
    }
    chunk_size_str += "\r\n";
    // 发送分块大小
    send(client_socket, chunk_size_str.c_str(), chunk_size_str.length(), 0);
    // 发送数据
    send(client_socket, event_data.c_str(), event_data.length(), 0);
    send(client_socket, "\r\n", 2, 0);

    epoll_event event;
    event.data.fd = client_socket;
    event.events = EPOLLOUT;
    epoll_ctl(epollfd, EPOLL_CTL_MOD, client_socket, &event);

    return true;
   
}

void handle_end_client(int client_socket) {

    // 发送结束块
    send(client_socket, "0\r\n\r\n", 5, 0);

    close(client_socket);
}


// std::string aaa;
// std::string event_type;
// std::string model_name = "deepseek-v3";
// 解析 data: 字段
std::string parseData(const std::string& lines, std::string& event_type, std::string& aaa) {
    nlohmann::json data_json;
    std::string data_str = "";
    
    size_t pos_event = lines.find("event:");
    size_t pos_data = lines.find("data:");
    if (pos_event != std::string::npos) {
        std::string eventPart = lines.substr(pos_event + 6);
        

        // 分割出来event部分
        size_t first = eventPart.find("\n");
        if (first != std::string::npos) {
            eventPart = eventPart.substr(0, first);
        }
        event_type = eventPart;
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
            // aaa += data_json["content"];
            data_str = data_json["content"];
        }
        
    }

    std::string start_thinking_flag = "```ys_think\n\n<icon>https://wy-static.wenxiaobai.com/bot-capability/prod/%E6%B7%B1%E5%BA%A6%E6%80%9D%E8%80%83.png</icon>\n\n<start>思考中...</start>\n\n";
    std::string end_thinking_flag = "<end>已深度思考";
    // 整理思考内容
    // 替换 start_thinking_flag
    size_t start_pos = data_str.find(start_thinking_flag);
    if (start_pos != std::string::npos) {
        data_str.replace(start_pos, start_thinking_flag.length(), "```thinking\n\n");
    }

    // 替换 end_thinking_flag
    size_t end_pos = data_str.find(end_thinking_flag);
    if (end_pos != std::string::npos) {
        data_str = "\n```\n";
    }


    // 处理客户端请求
    std::vector<nlohmann::json> choices;
    nlohmann::json delta1 = {
        {"role", "assistant"},
        {"content", data_str}
    };
    nlohmann::json delta = {
        {"delta", delta1}
    };
    choices.push_back(delta);
    nlohmann::json data = {
        {"choices", choices},
        {"model", "deepseek"}
    };
    aaa += data_str;
    return data.dump();
}


int chat(std::string query, std::string model, vector<string> token1, int epollfd, int client_fd, std::string &aaa, bool use_search=false) {
    if(use_search){
        std::cout<<"使用搜索"<<std::endl;
    }
    std::string url = "api-bj.wenxiaobai.com";
    // std::string query = "详细介绍鲁迅";
    // std::string user_id = "103452339";
    std::string conversation_id = "";
    bool is_new_conversation = true;
    int turn_index = 0;
    bool use_think = model == "deepseek-r1" ? true : false;
    // bool use_search = true;
    std::vector<std::string> file_arr;
    std::string token(token1[0]);
    std::string user_id(token1[1]);
    std::string device_id(token1[2]);
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
    std::string body_str = body.dump();
    // 构建请求头
    std::string header = jsonToSocketHeader(body_str, token, device_id);
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
        // std::string aaa;
        std::string event_type;
        const int bufferSize = 4096;
        char buffer[bufferSize];
        // std::string line;
        std::string receivedData; // 存储接收到的所有数据
        int bytesRead;
        int a = 0;
        handle_start_client(client_fd);
        bool is_connect = true;
        while ((bytesRead = SSL_read(ssl, buffer, bufferSize - 1)) > 0 && event_type!="close" && is_connect) {
            // bytesRead = SSL_read(ssl, buffer, bufferSize - 1);
            if (bytesRead > 0) {
                buffer[bytesRead] = '\0';
                receivedData += buffer;

                std::regex pattern(R"(\r\n.*\r\n)"); // 
                std::string result = std::regex_replace(receivedData, pattern, "");
                receivedData = result;
                size_t pos = receivedData.find("\n\n");
                while (pos != std::string::npos) {
                    std::string completeMessage = receivedData.substr(0, pos);
                    std::string data_str = parseData(completeMessage, event_type, aaa);
                    is_connect = handle_client(epollfd, client_fd, "data: " + data_str + "\n\n");
                    receivedData = receivedData.substr(pos + 2);
                    pos = receivedData.find("\n\n");
                }
            } else if (bytesRead == 0  ) {
                // 处理最后一行
                // if (!receivedData.empty()) {
                //     parseData(receivedData);
                // }
                // std::cout << "Connection closed by server." << std::endl;
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
        handle_end_client(client_fd);

        // 输出响应
        
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


    return 0;
}




std::string getMySQLDateTime() {
    std::time_t now = std::time(nullptr);
    std::tm* localTime = std::localtime(&now);

    std::stringstream ss;
    ss << std::put_time(localTime, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}



// 检查字符是否为 Emoji
bool isEmoji(char32_t c) {
    return (c >= 0x1F300 && c <= 0x1F5FF) || 
           (c >= 0x1F600 && c <= 0x1F64F) || 
           (c >= 0x1F680 && c <= 0x1F6FF) || 
           (c >= 0x2600 && c <= 0x26FF) || 
           (c >= 0x2700 && c <= 0x27BF);
}

std::string escapeString(const std::string& input) {
    std::string result;
    for (char c : input) {
        if (c == '\'' || c == '\"') {
            result += '\\';
        }
        result += c;
    }
    return result;
}

// 去除字符串中的 Emoji
std::string removeEmojis(const std::string& input) {
    return input;
    // std::wstring_convert<std::codecvt_utf8<char32_t>, char32_t> converter;
    // std::u32string u32Str = converter.from_bytes(input);
    // std::u32string result;

    // for (char32_t c : u32Str) {
    //     if (!isEmoji(c)) {
    //         result += c;
    //     }
    // }

    // std::string a1 = converter.to_bytes(result);
    // std::string a2 = escapeString(a1);
    // return a2;
}

