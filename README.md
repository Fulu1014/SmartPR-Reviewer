# SmartPR-Reviewer

SmartPR-Reviewer 是一个使用 C++ 编写的 GitHub 自动化代码审查工具。它通过监听 Webhook 接收 GitHub Pull Request 事件，提取代码增量（Diff），并调用大语言模型（LLM）API 进行代码逻辑分析，最后将分析建议作为评论自动发布到对应的 PR 中。

## ⚙️ 核心功能

- **GitHub App 鉴权模式**：通过 OpenSSL 读取私钥（`.pem`）生成 JWT，动态换取 GitHub Installation Token，避免使用静态 Personal Access Token 带来的安全风险。
- **Webhook 签名校验**：使用 HMAC-SHA256 算法对接收到的 Webhook 请求体进行哈希校验，物理拦截未授权的伪造请求。
- **差异代码过滤**：解析 GitHub 提供的完整 Diff 数据，通过文件后缀名匹配过滤掉静态资源与普通文档，仅提取主流编程语言的变更片段发送给大模型。
- **守护进程支持**：支持通过 Linux POSIX API 将程序转入后台以守护进程（Daemon）模式静默运行，并实现标准输入输出的日志重定向。

## 🛠️ 技术栈

- **编程语言**：C++11
- **网络服务**：`cpp-httplib` (提供 HTTP Webhook 监听与外部 API 请求)
- **安全与加密**：`OpenSSL` (用于 HMAC-SHA256 签名校验与 RSA-SHA256 JWT 生成)
- **数据处理**：`nlohmann/json` (用于解析与组装深层嵌套的 JSON 数据)
- **操作系统 API**：Linux POSIX (进程控制与文件系统操作)

## 📦 编译与部署

### 1. 环境依赖

在 Ubuntu/Debian 环境下，请确保安装了构建工具和 OpenSSL 库：

```bash
sudo apt update
sudo apt install build-essential cmake libssl-dev
```

### 2. 获取代码与编译

```bash
git clone [https://github.com/您的用户名/SmartPR-Reviewer.git](https://github.com/您的用户名/SmartPR-Reviewer.git)
cd SmartPR-Reviewermkdir build && cd build
cmake ..
make -j4
```

## 📝 运行配置

在运行或编译前，请修改 `include/config.h` 中的参数，填入你的专属应用信息：

```c++
namespace Config {
    // 填入你的 GitHub App ID (纯数字)
    const std::string APP_ID = "123456"; 
    
    // 填入私钥文件的绝对路径
    const std::string PRIVATE_KEY_PATH = "/绝对路径/app-private-key.pem"; 
    
    // GitHub Webhook 的安全校验码
    const std::string GITHUB_WEBHOOK_SECRET = "您的_Secret";
    
    // 大模型服务商的 API Key
    const std::string SILICONFLOW_API_KEY = "您的_API_KEY";
    
    const std::string LLM_API_URL = "[https://api.siliconflow.cn/v1/chat/completions](https://api.siliconflow.cn/v1/chat/completions)";
    const std::string LLM_MODEL = "deepseek-ai/DeepSeek-Coder-V2-Instruct"; 
}
```

## 🚀 使用指南

编译完成后，在 `build` 目录下可使用以下命令启动服务：

**前台运行（查看实时日志，指定端口为 8080）：**

```bash
./smartpr_reviewer -p 8080
```

**后台守护进程模式运行：**

```bash
./smartpr_reviewer -d -p 8080
```

在后台运行时，可以通过 **tail -f smartpr_reviewer.log** 动态查看输出日志。

## 🔄 核心工作流

1. **接收推送**：接收 GitHub 触发的 `pull_request` Webhook。
2. **安全核验**：对比 `X-Hub-Signature-256` 签名头。
3. **获取令牌**：利用 JWT 向 GitHub API 请求有效期 1 小时的临时 Token。
4. **代码提取**：调用 API 获取 PR 涉及的所有文件差异，清洗并拼接代码。
5. **模型审查**：将代码发送至大语言模型进行漏洞或逻辑评估。
6. **结果回传**：将评估结果以 Issue Comment 形式推送到 GitHub 仓库。