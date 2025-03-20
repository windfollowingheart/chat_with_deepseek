#include <iostream>
#include <string>
#include <map>
#include <cstring> // 用于 strlen 和 strstr
#include <cstdio>

std::string makeControlCharsVisible(const std::string& str) {
    std::string result;
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\r' && i + 1 < str.size() && str[i + 1] == '\n') {
            result += "[CR][LF]"; // 将 \r\n 替换为 [CR][LF]
            ++i; // 跳过 \n
        } else if (str[i] == '\r') {
            result += "[CR]"; // 单独的 \r
        } else if (str[i] == '\n') {
            result += "[LF]"; // 单独的 \n
        } else {
            result += str[i]; // 普通字符
        }
    }
    return result;
}

// 解析表单数据
void parseFormData(const std::string& data, const std::string& BOUNDARY) {
    std::string otherParam;
    std::string fileContent;

    size_t otherParamStart = data.find("name=\"otherParam\"");
    if (otherParamStart != std::string::npos) {
        otherParamStart = data.find("\r\n\r\n", otherParamStart) + 4;
        size_t otherParamEnd = data.find("\r\n--" + std::string(BOUNDARY), otherParamStart);
        otherParam = data.substr(otherParamStart, otherParamEnd - otherParamStart);
    }

    size_t fileStart = data.find("name=\"file\"");
    if (fileStart != std::string::npos) {
        fileStart = data.find("\r\n\r\n", fileStart) + 4;
        size_t fileEnd = data.find("\r\n--" + std::string(BOUNDARY), fileStart);
        fileContent = data.substr(fileStart, fileEnd - fileStart);
        std::cout <<  (fileStart == std::string::npos) <<std::endl;
        std::cout <<  (fileStart == std::string::npos) <<std::endl;
        std::cout <<  data.size() <<std::endl;
        // std::cout << makeControlCharsVisible(fileContent) <<std::endl;
    }

    std::cout << "其他参数: " << otherParam << std::endl;

    // 将文件内容写入本地
    std::ofstream outputFile("received.pdf", std::ios::binary);
    if (outputFile) {
        outputFile << fileContent;
        outputFile.close();
        std::cout << "文件已成功保存为 received.pdf" << std::endl;
    } else {
        std::cerr << "无法打开文件以保存内容" << std::endl;
    }
}



// 解析 multipart/form-data 格式的字符串
std::map<std::string, std::string> parseMultipartFormData1(const std::string& data, std::string& boundary) {
    std::map<std::string, std::string> result;


    size_t boundaryStart = data.find("--" + boundary); // 查找边界开始
    if (boundaryStart == std::string::npos) {
        return result; // 没有找到边界，返回空结果
    }

    // 循环解析每个部分
    while (boundaryStart != std::string::npos) {
        size_t boundaryEnd = data.find("\r\n", boundaryStart);
        if (boundaryEnd == std::string::npos) {
            break;
        }

        // 解析头部信息
        size_t headersStart = boundaryEnd + 2;
        size_t headersEnd = data.find("\r\n\r\n", headersStart);
        if (headersEnd == std::string::npos) {
            break;
        }

        // 提取头部信息
        std::string headers = data.substr(headersStart, headersEnd - headersStart);

        // 解析头部中的文件名和字段名
        size_t nameStart = headers.find("name=\"");
        size_t filenameStart = headers.find("filename=\"");
        size_t fileContentTypeStart = headers.find("Content-Type: ");
        std::string name, filename, fileContentType;

        if (nameStart != std::string::npos) {
            nameStart += 6;
            size_t nameEnd = headers.find("\"", nameStart);
            name = headers.substr(nameStart, nameEnd - nameStart);
        }

        if (filenameStart != std::string::npos) {
            filenameStart += 10;
            size_t filenameEnd = headers.find("\"", filenameStart);
            filename = headers.substr(filenameStart, filenameEnd - filenameStart);
        }

        if (fileContentTypeStart != std::string::npos) {
            fileContentTypeStart += 14;
            size_t fileContentTypeEnd = headers.find("\"", fileContentTypeStart);
            fileContentType = headers.substr(fileContentTypeStart, fileContentTypeEnd - fileContentTypeStart);
        }

        // 提取内容
        size_t contentStart = headersEnd + 4;
        size_t contentEnd = data.find("\r\n--" + boundary, contentStart);
        std::string content = data.substr(contentStart, contentEnd - contentStart);

        // 存储结果
        if (!name.empty()) {
            result[name] = content;
        }

        
        if (!filename.empty()) {
            result["filename"] = filename;
            result["file_content_type"] = fileContentType;
            result["file_content"] = content;
            // result["file_start"] = std::to_string(contentStart);
            // result["file_end"] = std::to_string(contentEnd - contentStart);
        }

        // 查找下一个边界
        boundaryStart = data.find("--" + boundary, contentEnd);
    }

    return result;
}

bool write_file(const std::string &data, const std::string &file_name){
    // 将文件内容写入本地
    std::ofstream outputFile(file_name, std::ios::binary);
    if (outputFile) {
        outputFile << data;
        outputFile.close();
        std::cout << "文件已成功保存为 " << file_name << std::endl;
    } else {
        std::cerr << "无法打开文件以保存内容" << std::endl;
    }
}


// 扩展内存
char* expand_buffer(char* buffer, size_t& current_size, size_t required_size) {
    // if (required_size > MAX_BUFFER_SIZE) {
    //     std::cerr << "Required size exceeds maximum buffer size." << std::endl;
    //     return nullptr;
    // }

    size_t new_size = current_size;
    while (new_size < required_size) {
        new_size *= 2; // 每次扩展为当前大小的 2 倍
    }

    char* new_buffer = new char[new_size]; // 分配新内存
    if (buffer) {
        memcpy(new_buffer, buffer, current_size); // 复制旧数据
        delete[] buffer; // 释放旧内存
    }

    current_size = new_size;
    return new_buffer;
}

const char* findLastNull(const char* str, int len1) {
    size_t length = strlen(str); // 获取字符串长度
    const char* p = str - length + len1; // 指向字符串的结尾
    while (p >= str) {
        if (*p == '\0') {
            return p+1; // 找到倒数第一个 '\0'
        }
        p--;
    }
    return nullptr; // 如果找不到，返回 nullptr
}



void writeFile(char* pdfData) {
    // 示例的 char* 数据（假设这是一个 PDF 文件的内容）
    size_t dataSize = strlen(pdfData); // 获取数据长度

    // 打开文件
    const char* filename = "output.pdf";
    FILE* file = fopen(filename, "wb");
    if (!file) {
        perror("Failed to open file");
    }

    // 写入数据
    size_t written = fwrite(pdfData, 1, dataSize, file);
    if (written != dataSize) {
        perror("Failed to write data");
        fclose(file);
    }

    // 关闭文件
    fclose(file);

    printf("PDF file written successfully: %s\n", filename);
}


