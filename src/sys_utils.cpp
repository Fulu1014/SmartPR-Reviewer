// src/sys_utils.cpp
#include "sys_utils.h"
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#include <iostream>

void SysUtils::RunAsDaemon(const std::string& log_file_path) {
    // 1. 第一次 fork：创建子进程，父进程退出。
    // 这会让终端认为命令已经执行完毕，返回命令提示符。
    pid_t pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // 父进程结束

    // 2. 创建一个新的会话 (Session)，让子进程脱离原有的终端控制
    if (setsid() < 0) exit(EXIT_FAILURE);

    // 3. 第二次 fork：确保进程不是会话首进程，防止它再次打开并独占一个终端
    pid = fork();
    if (pid < 0) exit(EXIT_FAILURE);
    if (pid > 0) exit(EXIT_SUCCESS); // 第一个子进程结束

    // 4. 将标准输出 (std::cout) 和标准错误 (std::cerr) 重定向到日志文件
    // 这样后台运行时的所有日志就不会丢失了
    freopen(log_file_path.c_str(), "a", stdout);
    freopen(log_file_path.c_str(), "a", stderr);

    std::cout << "\n=============================================" << std::endl;
    std::cout << "[System] Process daemonized successfully." << std::endl;
}