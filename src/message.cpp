#include "ros/message.hpp"

#include <utility>

#include "ros/exception.hpp"
#include "ros/util.hpp"

namespace ros_sim {

Message::Message(std::string content) : content_(std::move(content)) {
    if (content_.empty()) {
        throw MessageException("消息内容不能为空");
    }
    timestamp_ = nowTimestamp();
}

std::string Message::toString() const {
    return content_ + " @ " + timestamp_;
}

} // namespace ros_sim
