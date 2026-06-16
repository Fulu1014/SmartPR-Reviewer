// src/http_utils.cpp
#include "http_utils.h"
#include <curl/curl.h>
#include <iostream>

// libcurl 接收数据的回调函数
static size_t WriteCallback(void *contents, size_t size, size_t nmemb, void *userp) {
    ((std::string*)userp)->append((char*)contents, size * nmemb);
    return size * nmemb;
}

std::string HttpUtils::Get(const std::string& url, const std::vector<std::string>& headers) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // 设置自定义 Headers
        struct curl_slist *chunk = NULL;
        for(const auto& header : headers) {
            chunk = curl_slist_append(chunk, header.c_str());
        }
        // GitHub API 强制要求设置 User-Agent
        chunk = curl_slist_append(chunk, "User-Agent: SmartPR-Reviewer-Bot");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        // 忽略 SSL 证书校验（为了本地测试方便，生产环境建议开启）
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
    }
    return readBuffer;
}

// 将这段代码追加到 src/http_utils.cpp 文件的最后
std::string HttpUtils::Post(const std::string& url, const std::vector<std::string>& headers, const std::string& body) {
    CURL *curl;
    CURLcode res;
    std::string readBuffer;

    curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // 声明这是一个 POST 请求，并传入 Body 数据
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        
        struct curl_slist *chunk = NULL;
        for(const auto& header : headers) {
            chunk = curl_slist_append(chunk, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &readBuffer);
        
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);

        res = curl_easy_perform(curl);
        if(res != CURLE_OK) {
            std::cerr << "curl_easy_perform() POST failed: " << curl_easy_strerror(res) << std::endl;
        }

        curl_easy_cleanup(curl);
        curl_slist_free_all(chunk);
    }
    return readBuffer;
}