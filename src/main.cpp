// src/main.cpp
#include <iostream>
#include <string>
#include <vector>
#include <unistd.h> 
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <openssl/hmac.h>
#include <openssl/evp.h>

#include "httplib.h"
#include "json.hpp"
#include "config.h"
#include "http_utils.h"
#include "llm_reviewer.h"
#include "github_client.h"
#include "sys_utils.h"

using json = nlohmann::json;

// 强力去除字符串首尾空格和幽灵字符（例如 \r, \n）
std::string Trim(const std::string& s) {
    std::string res = s;
    res.erase(res.begin(), std::find_if(res.begin(), res.end(), [](unsigned char ch) { return !std::isspace(ch); }));
    res.erase(std::find_if(res.rbegin(), res.rend(), [](unsigned char ch) { return !std::isspace(ch); }).base(), res.end());
    return res;
}

// 打印命令行帮助信息
void PrintHelp(const char* prog_name) {
    std::cout << "Usage: " << prog_name << " [-d] [-p PORT]\n"
              << "Options:\n"
              << "  -d        Run in background as a daemon.\n"
              << "  -p PORT   Specify the listening port (default: 8080).\n"
              << "  -h        Show this help message.\n";
}

// 检查文件是否属于我们支持审查的主流编程语言
bool IsCodeFile(const std::string& filename) {
    std::vector<std::string> valid_extensions = {
        ".c", ".cpp", ".cc", ".h", ".hpp", 
        ".py", ".java", ".js", ".ts", ".jsx", ".tsx", 
        ".go", ".rs", ".cs", ".rb", ".php", ".swift", ".kt", ".sh"
    };
    
    size_t dot_pos = filename.find_last_of('.');
    if (dot_pos == std::string::npos) return false;
    
    std::string ext = filename.substr(dot_pos);
    for (const auto& valid_ext : valid_extensions) {
        if (ext == valid_ext) return true;
    }
    return false;
}

// 【安全模块】HMAC-SHA256 签名计算函数
std::string CalculateHMACSHA256(const std::string& payload, const std::string& secret) {
    unsigned char* digest;
    unsigned int digest_len = 0;
    
    digest = HMAC(EVP_sha256(), secret.c_str(), secret.length(),
                  (unsigned char*)payload.c_str(), payload.length(), NULL, &digest_len);
    
    std::stringstream ss;
    for(unsigned int i = 0; i < digest_len; i++) {
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)digest[i];
    }
    return "sha256=" + ss.str();
}

int main(int argc, char* argv[]) {
    bool run_as_daemon = false;
    int port = 8080;
    int opt;

    // 解析命令行参数
    while ((opt = getopt(argc, argv, "dp:h")) != -1) {
        switch (opt) {
            case 'd': run_as_daemon = true; break;
            case 'p': port = std::stoi(optarg); break;
            case 'h': PrintHelp(argv[0]); return 0;
            default: PrintHelp(argv[0]); return 1;
        }
    }

    // 后台挂起模式
    if (run_as_daemon) {
        std::cout << "Starting SmartPR-Reviewer in background..." << std::endl;
        SysUtils::RunAsDaemon("smartpr_reviewer.log");
    }

    httplib::Server svr;
    std::cout << "🚀 SmartPR-Reviewer Webhook Server listening on 0.0.0.0:" << port << "..." << std::endl;

    // 配置 Webhook 监听路由
    svr.Post("/webhook", [](const httplib::Request& req, httplib::Response& res) {
        std::cout << "\n========== [New Webhook Event Received] ==========" << std::endl;
        
        // 【防御第一线】：执行 GitHub Webhook 签名安全校验
        if (req.has_header("X-Hub-Signature-256")) {
            std::string github_sig = req.get_header_value("X-Hub-Signature-256");
            std::string local_sig = CalculateHMACSHA256(req.body, Config::GITHUB_WEBHOOK_SECRET);
            
            if (github_sig != local_sig) {
                std::cerr << "[Security Alert] Invalid signature! Attack intercepted. Expected: " << local_sig << " but got: " << github_sig << std::endl;
                res.status = 401; 
                res.set_content("{\"status\": \"unauthorized\", \"message\": \"Signature mismatch\"}", "application/json");
                return; // 拦截并终止执行
            }
            std::cout << "[Security] Webhook signature verified successfully! Payload is authentic." << std::endl;
        } else {
            std::cerr << "[Security Alert] Missing signature header. Request rejected." << std::endl;
            res.status = 401;
            res.set_content("{\"status\": \"unauthorized\", \"message\": \"Missing signature\"}", "application/json");
            return;
        }

        try {
            json payload = json::parse(req.body);

            // 判断是否为 Pull Request 事件
            if (payload.contains("pull_request")) {
                std::string action = payload["action"].get<std::string>();
                
                // 只处理新建或代码有同步的 PR
                if (action == "opened" || action == "synchronize") {
                    std::string raw_repo_name = payload["repository"]["full_name"].get<std::string>();
                    std::string repo_full_name = Trim(raw_repo_name); 
                    int pr_number = payload["pull_request"]["number"];
                    
                    // 【GitHub App 架构】：提取触发本次 Webhook 的安装实例 ID
                    if (!payload.contains("installation") || !payload["installation"].contains("id")) {
                        std::cerr << "[Error] Webhook missing installation ID. Make sure this is sent via GitHub App." << std::endl;
                        res.set_content("{\"status\": \"ignored\"}", "application/json");
                        return;
                    }
                    int installation_id = payload["installation"]["id"];
                    
                    std::cout << "[Target Acquired] Repo: " << repo_full_name << " | PR: #" << pr_number << " | App Install ID: " << installation_id << std::endl;

                    // 【核心鉴权】：使用私钥换取该仓库的专属动态 Token
                    std::string dynamic_token = GitHubClient::GetInstallationToken(installation_id);
                    if (dynamic_token.empty()) {
                        std::cerr << "[System Error] Aborting workflow due to Auth failure." << std::endl;
                        res.status = 401;
                        return;
                    }

                    // 开始拉取结构化的 Diff 文件列表
                    std::string files_url = "https://api.github.com/repos/" + repo_full_name + "/pulls/" + std::to_string(pr_number) + "/files";
                    std::vector<std::string> headers = {
                        "Authorization: Bearer " + dynamic_token,
                        "Accept: application/vnd.github.v3+json",
                        "User-Agent: SmartPR-Reviewer-App"
                    };

                    std::cout << "[GitHub] Fetching PR Files list..." << std::endl;
                    std::string files_resp = HttpUtils::Get(files_url, headers);

                    if (files_resp.empty()) {
                        std::cerr << "[Error] Failed to fetch PR files." << std::endl;
                    } else {
                        std::string final_diff_content = "";
                        int filtered_file_count = 0;

                        // 过滤非代码文件，拼接需要审查的 Diff
                        try {
                            json files_json = json::parse(files_resp);
                            for (const auto& file : files_json) {
                                std::string filename = file["filename"];
                                if (IsCodeFile(filename) && file.contains("patch")) {
                                    final_diff_content += "File: " + filename + "\n";
                                    final_diff_content += file["patch"].get<std::string>() + "\n\n";
                                    filtered_file_count++;
                                }
                            }
                        } catch (const std::exception& e) {
                            std::cerr << "[Error] JSON parse error: " << e.what() << std::endl;
                        }

                        // 如果存在有效的代码变更，丢给大模型分析
                        if (!final_diff_content.empty()) {
                            std::cout << "[LLM] Analyzing code vulnerabilities..." << std::endl;
                            std::string review_comment = LLMReviewer::AnalyzeDiff(final_diff_content);

                            if (!review_comment.empty()) {
                                std::cout << "\n========== [DeepSeek Review Result] ==========\n";
                                std::cout << review_comment << std::endl;
                                std::cout << "==============================================\n";
                                
                                // 回传发布到 GitHub
                                GitHubClient::PostComment(repo_full_name, pr_number, review_comment, dynamic_token);
                            } else {
                                std::cerr << "[Warning] Review generated an error or is empty. Skipping GitHub comment." << std::endl;
                            }
                        } else {
                            std::cout << "[Info] No supported code files changed. Skipping LLM review." << std::endl;
                        }
                    }
                } else {
                    std::cout << "Ignoring PR action: " << action << std::endl;
                }
            } else {
                std::cout << "Not a Pull Request event. Ignoring." << std::endl;
            }

            res.set_content("{\"status\": \"success\"}", "application/json");
            
        } catch (const std::exception& e) {
            std::cerr << "[System Error] Failed to process webhook: " << e.what() << std::endl;
            res.status = 500;
        }
    });

    svr.listen("0.0.0.0", port);
    return 0;
}