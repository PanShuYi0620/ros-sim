#pragma once
//表示该文件仅能使用一次
#include <fstream>
#include <string>

namespace ros_sim {

enum class LogLevel { Info, Warn, Error };
//描述了日志三种状态：正常，警告，出错

class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    ~Logger() = default;
    //构造函数私有，外面 new 不出来，只能通过 instance() 拿

    static const char* levelName(LogLevel level);
    static std::string nowTimestamp();
    std::ofstream file_;
};

}  // namespace ros_sim