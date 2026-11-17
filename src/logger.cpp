#include "ros/logger.hpp"
//引入在include，ros，logger里面写的头文件
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ros_sim {

// 1，单例：全局只有一个 Logger
Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

// 2，枚举转字符串
const char* Logger::levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Info:  return "INFO";
    case LogLevel::Warn:  return "WARN";
    case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

// 3，生成 "2026-11-15 19:10:00"
std::string Logger::nowTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}
// 4，核心：拼一行，打到屏幕，再写进文件

void Logger::log(LogLevel level, const std::string& message) {
    const std::string line =
        "[" + nowTimestamp() + "] [" + levelName(level) + "] " + message;

    if (consoleEnabled_) {
        std::cout << line << std::endl;
    }

    if (!file_.is_open()) {
        std::filesystem::create_directories("logs");
        file_.open("logs/ros_sim.log", std::ios::app);
    }
    if (file_.is_open()) {
        file_ << line << std::endl;
    }
}

// 5，三个便捷方法，都转发给 log
void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warn(const std::string& message) {
    log(LogLevel::Warn, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

}  // namespace ros_sim