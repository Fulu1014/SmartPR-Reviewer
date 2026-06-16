cmake_minimum_required(VERSION 3.10)
project(SmartPR_Reviewer)

set(CMAKE_CXX_STANDARD 17)

find_package(CURL REQUIRED)
find_package(Threads REQUIRED)
# 【新增】：寻找系统中的 OpenSSL 库
find_package(OpenSSL REQUIRED)

include_directories(include)

file(GLOB SOURCES "src/*.cpp")

add_executable(smartpr_reviewer ${SOURCES})

# 【新增】：链接 OpenSSL::Crypto 进行加密哈希计算
target_link_libraries(smartpr_reviewer ${CURL_LIBRARIES} Threads::Threads OpenSSL::SSL OpenSSL::Crypto)