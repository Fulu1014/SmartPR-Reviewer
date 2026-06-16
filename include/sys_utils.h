// include/sys_utils.h
#pragma once
#include <string>

class SysUtils {
public:
    // 将当前进程转为后台守护进程，并将日志输出到指定文件
    static void RunAsDaemon(const std::string& log_file_path);
};