#include "ros/message.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

#include "ros/exception.hpp"

namespace ros_sim {

Message::Message(std::string content) : content_(std::move(content)) {
    if (content_.empty()) {
        throw MessageException("消息内容不能为空");
    }
    timestamp_ = makeTimestamp();
}

std::string Message::makeTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string Message::toString() const {
    return content_ + " @ " + timestamp_;
}

} // namespace ros_sim
