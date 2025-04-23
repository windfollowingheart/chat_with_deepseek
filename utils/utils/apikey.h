#include <iostream>
#include <sstream>
#include <string>
#include <random>
#include <chrono>
#include <hiredis/hiredis.h>

std::string generateAuthorizedMessage(const std::string& apikey) {
    std::ostringstream oss;
    oss << "授权成功<br>\n";
    oss << "您的免费API Key为: " << apikey << "<br>\n";
    oss << "请妥善保管，不要泄露给他人，如泄漏造成滥用可能会导致Key被封禁";
    return oss.str();
}

void dcr_apikey(redisContext* redis, char* m_apikey){
    // 选择指定的数据库
    // std::cerr << "执行 SELECT 命令时出错qq11" << std::endl;
    // redisReply* reply = static_cast<redisReply*>(redisCommand(redis, "SELECT %d", 3));
    // if (reply == nullptr) {
    //     std::cerr << "执行 SELECT 命令时出错qq" << std::endl;
    // }
    redisReply* reply;
    // std::cerr << m_apikey << std::endl;
    reply = static_cast<redisReply*>(redisCommand(redis, "GET %s", m_apikey));
    if (reply == nullptr) {
        std::cerr << "Error executing command" << std::endl;
    }
    // std::cerr << m_apikey << std::endl;
    if (reply->type == REDIS_REPLY_NIL) {
        std::cerr << "No this apikey" << std::endl;
        std::cerr << m_apikey << std::endl;
    }
    if(reply->type == REDIS_REPLY_STRING){
        int value = std::stoi(reply->str);
        --value;
        reply = static_cast<redisReply*>(redisCommand(redis, "SET %s %d", m_apikey, value));
    }
    // freeReplyObject(reply);
    
}



// 生成 API Key 并存储到传入的字符数组中
void gen_api_key(char apikey[100]) {
    // 定义字符集，包含字母和数字
    const std::string alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    // 除去 "sk-" 后的长度
    const int length = 48;

    // 使用当前时间作为随机数生成器的种子
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine generator(seed);
    std::uniform_int_distribution<int> distribution(0, alphabet.length() - 1);

    // 生成随机部分
    std::string random_part;
    for (int i = 0; i < length; ++i) {
        random_part += alphabet[distribution(generator)];
    }

    // 组合成类似 OpenAI API Key 的格式
    std::string api_key_str = "sk-" + random_part;

    // 将生成的 API Key 复制到传入的字符数组中
    if (api_key_str.length() < 100) {
        for (size_t i = 0; i < api_key_str.length(); ++i) {
            apikey[i] = api_key_str[i];
        }
        apikey[api_key_str.length()] = '\0'; // 添加字符串结束符
    } else {
        std::cerr << "生成的 API Key 长度超过了字符数组的容量。" << std::endl;
        apikey[0] = '\0'; // 置为空字符串
    }
}

