#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "../nolhmann/json.h"

// 引入 nlohmann/json 命名空间
using json = nlohmann::json;

// 获取当前的 UTC 时间并转换为符合 RFC 1123 标准的字符串
std::string get_datetime() {
    std::time_t now = std::time(nullptr);
    std::tm* tm_info = std::gmtime(&now);

    std::ostringstream oss;
    oss << std::put_time(tm_info, "%a, %d %b %Y %H:%M:%S GMT");
    return oss.str();
}

// 计算 SHA-256 哈希值
std::string sha256_hash(const std::string& s) {
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, s.c_str(), s.length());
    SHA256_Final(hash, &sha256);

    BIO* bio = BIO_new(BIO_f_base64());
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, bmem);
    BIO_write(bio, hash, SHA256_DIGEST_LENGTH);
    BIO_flush(bio);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(bio);

    return result;
}

// 计算 HMAC-SHA1
std::string hmac_sha1(const std::string& key, const std::string& msg) {
    unsigned char digest[EVP_MAX_MD_SIZE];
    unsigned int digest_len;
    HMAC(EVP_sha1(), key.c_str(), key.length(), reinterpret_cast<const unsigned char*>(msg.c_str()), msg.length(), digest, &digest_len);

    BIO* bio = BIO_new(BIO_f_base64());
    BIO_set_flags(bio, BIO_FLAGS_BASE64_NO_NL);
    BIO* bmem = BIO_new(BIO_s_mem());
    bio = BIO_push(bio, bmem);
    BIO_write(bio, digest, digest_len);
    BIO_flush(bio);
    BUF_MEM* bptr;
    BIO_get_mem_ptr(bio, &bptr);
    std::string result(bptr->data, bptr->length);
    BIO_free_all(bio);

    return result;
}

// 获取签名信息
json get_sign(const std::string& body) {
    std::string data1 = body;
    std::string key = "TkoWuEN8cpDJubb7Zfwxln16NQDZIc8z";
    std::string data1_base64_encoded = sha256_hash(data1);
    std::string x_date = get_datetime();

    // x_date = "Tue, 18 Feb 2025 13:51:38 GMT";
    std::string data2 = "x-date: " + x_date + "\ndigest: SHA-256=" + data1_base64_encoded;
    std::string data2_base64_encoded = hmac_sha1(key, data2);
    std::string digest = "SHA-256=" + data1_base64_encoded;
    std::string authorization = "hmac username=\"web.1.0.beta\", algorithm=\"hmac-sha1\", headers=\"x-date digest\", signature=\"" + data2_base64_encoded + "\"";

    return {
        {"digest", digest},
        {"authorization", authorization},
        {"x_date", x_date}
    };
}

// 获取请求头
json get_header(const std::string& body, const std::string& token) {
    json sign_res = get_sign(body);
    std::string digest = sign_res["digest"];
    std::string authorization = sign_res["authorization"];
    std::string x_date = sign_res["x_date"];

    return {
        {"Host", "api-bj.wenxiaobai.com"},
        {"Connection", "keep-alive"},
        {"X-Yuanshi-Platform", "web"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"Authorization", authorization},
        {"X-Yuanshi-DeviceMode", "Edge"},
        {"sec-ch-ua", "\"Not(A:Brand\";v=\"99\", \"Microsoft Edge\";v=\"133\", \"Chromium\";v=\"133\""},
        {"sec-ch-ua-mobile", "?0"},
        {"X-Yuanshi-Channel", "browser"},
        {"X-Yuanshi-DeviceOS", "133"},
        {"Digest", digest},
        {"Accept", "text/event-stream, text/event-stream"},
        {"X-Yuanshi-AppVersionCode", ""},
        {"Content-type", "application/json"},
        {"x-date", x_date},
        {"X-Yuanshi-Authorization", "Bearer " + token},
        {"X-Yuanshi-DeviceId", "htsclatat8idiem7yq2d10xu5imca9ie_0hihtqfzn266h_ogwj0u"},
        {"X-Yuanshi-TimeZone", "Asia/Shanghai"},
        {"X-Yuanshi-AppVersionName", "3.1.0"},
        {"User-Agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/133.0.0.0 Safari/537.36 Edg/133.0.0.0"},
        {"X-Yuanshi-AppName", "wenxiaobai"},
        {"X-Yuanshi-Locale", "zh"},
        {"Origin", "https://www.wenxiaobai.com"},
        {"Sec-Fetch-Site", "same-site"},
        {"Sec-Fetch-Mode", "cors"},
        {"Sec-Fetch-Dest", "empty"},
        {"Referer", "https://www.wenxiaobai.com/"},
        {"Accept-Encoding", "gzip, deflate, br, zstd"},
        {"Accept-Language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"}
    };
}

// 将 json 格式的请求头转换为 Socket 格式的字符串
// std::string jsonToSocketHeader(const nlohmann::json& headers) {
std::string jsonToSocketHeader(const std::string& body, const std::string& token) {
    const nlohmann::json headers = get_header(body, token);
    std::string socketHeader;
    socketHeader += "POST /api/v1.0/core/conversation/chat/v1 HTTP/1.1\r\n";
    for (const auto& [key, value] : headers.items()) {
        socketHeader += key + ": " + value.get<std::string>() + "\r\n";
    }
    socketHeader += "Content-Length: " + std::to_string(body.length()) + "\r\n";
    // 最后添加一个空行，表示请求头结束
    socketHeader += "\r\n";
    return socketHeader;
}

// int main() {
//     // std::string body = "your_body_string";
//     // std::string token = "your_token";
//     // json header = get_header(body, token);
//     json aa = get_sign("");
//     std::cout << aa.dump(4) << std::endl;
//     return 0;
// }