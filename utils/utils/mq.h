#include <string>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <iostream>
#include <sstream>
#include <vector>

std::string gen_uuid() {
    // 创建一个随机 UUID 生成器
    boost::uuids::random_generator gen;

    // 生成一个 UUID
    boost::uuids::uuid uuid = gen();

    std::string uuidStr = boost::uuids::to_string(uuid);

    return uuidStr;
}



std::vector<std::string> splitString(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    while (std::getline(ss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}