// include/http_utils.h
#pragma once
#include <string>
#include <vector>

class HttpUtils {
public:
    // 发送带有自定义 Header 的 GET 请求
    static std::string Get(const std::string& url, const std::vector<std::string>& headers);
    
    // 新增：发送带有自定义 Header 和 JSON Body 的 POST 请求
    static std::string Post(const std::string& url, const std::vector<std::string>& headers, const std::string& body);
};