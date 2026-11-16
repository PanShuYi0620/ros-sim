#include "ros/sensor_message.hpp"

#include <iomanip>
#include <sstream>
#include <utility>

namespace ros_sim {

SensorMessage::SensorMessage(std::string content, double temperature, double humidity)
    : Message(std::move(content)), temperature_(temperature), humidity_(humidity) {}

std::string SensorMessage::toString() const {
    // 先拿基类的结果（内容 + 时间戳），再拼上自己的字段
    std::ostringstream oss;
    oss << Message::toString() << " | 温度=" << std::fixed << std::setprecision(1)
        << temperature_ << " 湿度=" << std::fixed << std::setprecision(1) << humidity_;
    return oss.str();
}

}  // namespace ros_sim
