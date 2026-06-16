// include/llm_reviewer.h
#pragma once
#include <string>

class LLMReviewer {
public:
    // 接收代码 Diff，返回大模型的审查意见（Markdown 格式）
    static std::string AnalyzeDiff(const std::string& diff_content);
};