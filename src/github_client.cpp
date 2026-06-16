// src/github_client.cpp
#include "github_client.h"
#include "http_utils.h"
#include "config.h"
#include "json.hpp"
#include <iostream>
#include <chrono>
#include <algorithm>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>

using json = nlohmann::json;

// 辅助函数：Base64Url 编码 (JWT 标准规范)
std::string Base64UrlEncode(const unsigned char* buffer, size_t length) {
    BIO *bio, *b64;
    BUF_MEM *bufferPtr;
    b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);
    BIO_write(bio, buffer, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);
    std::string result(bufferPtr->data, bufferPtr->length);
    BIO_free_all(bio);

    // 将标准 Base64 转换为 Base64Url 格式 (+变-, /变_, 去掉=)
    for (char& c : result) {
        if (c == '+') c = '-';
        else if (c == '/') c = '_';
    }
    result.erase(std::remove(result.begin(), result.end(), '='), result.end());
    return result;
}

std::string GitHubClient::GetInstallationToken(int installation_id) {
    // 1. 构造 JWT 头部 (固定为 RS256)
    std::string header_b64 = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";

    // 2. 构造 JWT 载荷 (包含 App ID 和有效期)
    auto now = std::chrono::system_clock::now();
    auto iat = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count() - 60; // 过去 60 秒应对时钟偏移
    auto exp = iat + 600; // 最长有效期 10 分钟
    std::string payload = "{\"iat\":" + std::to_string(iat) + ",\"exp\":" + std::to_string(exp) + ",\"iss\":\"" + Config::APP_ID + "\"}";
    std::string payload_b64 = Base64UrlEncode((const unsigned char*)payload.c_str(), payload.length());

    std::string sign_input = header_b64 + "." + payload_b64;

    // 3. 读取私钥 (.pem) 并进行 RSA-SHA256 签名
    FILE* key_file = fopen(Config::PRIVATE_KEY_PATH.c_str(), "r");
    if (!key_file) {
        std::cerr << "[Auth Error] Could not open private key file: " << Config::PRIVATE_KEY_PATH << std::endl;
        return "";
    }
    EVP_PKEY* pkey = PEM_read_PrivateKey(key_file, NULL, NULL, NULL);
    fclose(key_file);
    
    if (!pkey) {
        std::cerr << "[Auth Error] Failed to read private key format." << std::endl;
        return "";
    }

    EVP_MD_CTX* md_ctx = EVP_MD_CTX_new();
    EVP_DigestSignInit(md_ctx, NULL, EVP_sha256(), NULL, pkey);
    EVP_DigestSignUpdate(md_ctx, sign_input.c_str(), sign_input.length());

    size_t sig_len = 0;
    EVP_DigestSignFinal(md_ctx, NULL, &sig_len);
    unsigned char* sig = new unsigned char[sig_len];
    EVP_DigestSignFinal(md_ctx, sig, &sig_len);

    std::string sig_b64 = Base64UrlEncode(sig, sig_len);

    delete[] sig;
    EVP_MD_CTX_free(md_ctx);
    EVP_PKEY_free(pkey);

    // 完整的 JWT 字符串
    std::string jwt = sign_input + "." + sig_b64;

    // 4. 发送请求，用 JWT 换取特定仓库的 Installation Token
    std::cout << "[GitHub Auth] Requesting dynamic Installation Token..." << std::endl;
    std::string url = "https://api.github.com/app/installations/" + std::to_string(installation_id) + "/access_tokens";
    std::vector<std::string> headers = {
        "Authorization: Bearer " + jwt,
        "Accept: application/vnd.github.v3+json",
        "User-Agent: SmartPR-Reviewer-App"
    };

    std::string response_str = HttpUtils::Post(url, headers, "");

    try {
        json response_json = json::parse(response_str);
        if (response_json.contains("token")) {
            std::cout << "[GitHub Auth] Successfully generated temporary token!" << std::endl;
            return response_json["token"];
        } else {
            std::cerr << "[GitHub Auth] Failed to exchange token. Response: " << response_str << std::endl;
            return "";
        }
    } catch (const std::exception& e) {
        std::cerr << "[GitHub Auth] JSON parse error: " << e.what() << std::endl;
        return "";
    }
}

bool GitHubClient::PostComment(const std::string& repo_full_name, int pr_number, const std::string& comment_body, const std::string& token) {
    std::string url = "https://api.github.com/repos/" + repo_full_name + "/issues/" + std::to_string(pr_number) + "/comments";
    json body_json = {{"body", comment_body}};

    // 使用动态传入的 token，不再读 config
    std::vector<std::string> headers = {
        "Authorization: Bearer " + token,
        "Accept: application/vnd.github.v3+json",
        "User-Agent: SmartPR-Reviewer-App"
    };

    std::cout << "[GitHub] Publishing review comment as GitHub App..." << std::endl;
    std::string response_str = HttpUtils::Post(url, headers, body_json.dump());

    try {
        json response_json = json::parse(response_str);
        if (response_json.contains("id")) {
            std::cout << "[GitHub] Successfully posted comment! Comment ID: " << response_json["id"] << std::endl;
            return true;
        } else {
            std::cerr << "[GitHub Error] Failed to post. Response: " << response_str << std::endl;
            return false;
        }
    } catch (...) {
        return false;
    }
}