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

# 核心库：除 main 外的所有实现，主程序和测试程序共用
add_library(ros_core STATIC
    src/logger.cpp
    src/message.cpp
    src/sensor_message.cpp
    src/node.cpp
    src/ros.cpp
    src/publisher.cpp
    src/subscriber.cpp
)
target_include_directories(ros_core PUBLIC include)

# 演示程序
add_executable(ros_sim
    src/main.cpp
)
target_link_libraries(ros_sim PRIVATE ros_core)

# 测试程序
add_executable(ros_tests
    tests/test_main.cpp
)
target_link_libraries(ros_tests PRIVATE ros_core)
