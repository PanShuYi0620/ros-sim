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

    // 是否同时输出到控制台（默认 true）。交互式控制台可以关掉，只看日志文件。
    void setConsoleEnabled(bool enabled) noexcept { consoleEnabled_ = enabled; }

private:
    Logger() = default;
    ~Logger() = default;
    //构造函数私有，外面 new 不出来，只能通过 instance() 拿

    static const char* levelName(LogLevel level);
    static std::string nowTimestamp();
    std::ofstream file_;
    bool consoleEnabled_{true};
};

}  // namespace ros_sim