// include/github_client.h
#pragma once
#include <string>

class GitHubClient {
public:
    // 核心安全方法：使用 App 私钥生成 JWT，并向 GitHub 换取特定仓库的临时操作 Token
    static std::string GetInstallationToken(int installation_id);

    // 发送评论 (新增了 token 参数，不再从 config.h 里读死数据)
    static bool PostComment(const std::string& repo_full_name, int pr_number, const std::string& comment_body, const std::string& token);
};