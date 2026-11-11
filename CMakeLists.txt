cmake_minimum_required(VERSION 3.16)

project(ros_sim VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 统一开启警告；MSVC 额外加 /utf-8，保证中文字符串按 UTF-8 编译
if(MSVC)
    add_compile_options(/W4 /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

add_executable(ros_sim
    src/main.cpp
)

target_include_directories(ros_sim PRIVATE include)
