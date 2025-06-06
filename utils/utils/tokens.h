#pragma once

#include <iostream>
#include <string>
#include <fstream>
#include <list>
#include <string>
#include <vector>
#include "../nolhmann/json.h"

list<vector<string>> getTokenList() {
    // 打开 JSON 文件
    list<vector<string>> tokenList;
    std::string path = "/home/wqt/projects/cpp/chat_with_deepseek/test/test_json/visitor_info.json";
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open file!" << std::endl;
        return tokenList;
    }

    try {
        // 解析 JSON 文件
        nlohmann::json json_data;
        file >> json_data; // 从文件读取 JSON 数据

        // 检查是否是 JSON 数组
        if (!json_data.is_array()) {
            std::cerr << "The JSON file does not contain an array!" << std::endl;
            return tokenList;
        }

        std::cout <<json_data.size()<<std::endl;
        list<vector<string>> tokenList;
        // 遍历 JSON 数组
        for (const auto& item : json_data) {
            // 假设数组中的每个元素是一个对象
            if (item.is_object()) {
                // 访问对象中的字段
                std::string token = item.value("token", "Unknown");
                int userid = item.value("userid", 0);
                std::string user_id = std::to_string(userid);
                std::string device_id = item.value("deviceId", "Unknown");
                // char* tokenCStr = strdup(token.c_str());
                vector<string> token1 = {token, user_id, device_id};
                // tokenList.push_back(tokenCStr);
                tokenList.push_back(token1);
                // int age = item.value("age", 0);
                // std::cout << "Name: " << name << ", Age: " << age << std::endl;
                // std::cout  << name << "\n" << std::endl;
            } else {
                std::cerr << "Array element is not an object!" << std::endl;
            }
        }
        return tokenList;
    } catch (const std::exception& e) {
        std::cerr << "JSON parsing error: " << e.what() << std::endl;
        return tokenList;
    }

    return tokenList;
}