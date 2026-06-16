// src/llm_reviewer.cpp
#include "llm_reviewer.h"
#include "config.h"
#include "http_utils.h"
#include "json.hpp"
#include <iostream>
#include <algorithm>

using json = nlohmann::json;

std::string LLMReviewer::AnalyzeDiff(const std::string& diff_content) {
    std::cout << "[LLM] Assembling optimized prompt for DeepSeek..." << std::endl;

    // 【优化 1】：输入截断防爆破。防止别人提了一个上万行的 PR，把你的 Token 一次性耗尽
    std::string safe_diff = diff_content;
    const size_t MAX_DIFF_LENGTH = 6000; // 限制最多只发约 6000 个字符
    if (safe_diff.length() > MAX_DIFF_LENGTH) {
        std::cout << "[Warning] Diff too long (" << safe_diff.length() << " chars). Truncating to save tokens." << std::endl;
        safe_diff = safe_diff.substr(0, MAX_DIFF_LENGTH) + "\n...[Diff over size limit, truncated]...";
    }

    // 构建发给大模型的 JSON 数据
    json request_body = {
        {"model", Config::LLM_MODEL},
        {"messages", json::array({
            {
                {"role", "system"},
                // 【优化 2】：极简 Prompt 约束。用强指令禁止大模型说废话
                {"content", "你是一个极其冷酷、高效的代码安全审查工具。请遵循以下绝对规则：\n"
                            "1. 拒绝废话：禁止任何开头问候、客套话或结尾总结。\n"
                            "2. 直击要害：如果代码有Bug或漏洞，只用一句话说明原因，然后立刻给出带有 ```suggestion 的修复代码块。\n"
                            "3. 保持静默：如果没有明显漏洞，只需输出四个字母 'LGTM'，绝对不要凑字数夸奖代码。"}
            },
            {
                {"role", "user"},
                {"content", safe_diff}
            }
        })},
        // 【优化 3】：降低温度（Temperature）提高确定性和速度
        {"temperature", 0.1},
        // 【优化 4】：强制限制最大输出 Token。哪怕它想啰嗦，到达 500 个 Token 也会被物理切断
        {"max_tokens", 500} 
    };

    std::vector<std::string> headers = {
        "Authorization: Bearer " + Config::SILICONFLOW_API_KEY,
        "Content-Type: application/json"
    };

    std::cout << "[LLM] Sending fast-request to SiliconFlow (DeepSeek)..." << std::endl;
    std::string response_str = HttpUtils::Post(Config::LLM_API_URL, headers, request_body.dump());

    try {
        json response_json = json::parse(response_str);
        
        if (response_json.contains("choices") && !response_json["choices"].empty()) {
            std::string content = response_json["choices"][0]["message"]["content"];
            
            // 如果模型按照我们的指令只回复了 LGTM，为了不打扰开发者，我们干脆拦截掉，不在 PR 里发废话
            if (content.find("LGTM") != std::string::npos && content.length() < 10) {
                std::cout << "[LLM] Code looks good (LGTM). Suppressing comment to avoid noise." << std::endl;
                return ""; 
            }
            return content;
        } else {
            std::cerr << "[LLM Error] API Gateway Error or blocked by safety filter." << std::endl;
            return ""; 
        }
    } catch (const std::exception& e) {
        std::cerr << "[LLM Error] JSON parse error: " << e.what() << std::endl;
        return ""; 
    }
}