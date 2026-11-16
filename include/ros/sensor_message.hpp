#pragma once

#include <string>

#include "ros/message.hpp"

namespace ros_sim {

// 自定义消息：在普通消息基础上多带温度和湿度
class SensorMessage : public Message {
public:
    SensorMessage(std::string content, double temperature, double humidity);

    double temperature() const noexcept { return temperature_; }
    double humidity() const noexcept { return humidity_; }

    // override：重写父类的打印方式，多输出两个字段
    std::string toString() const override;

private:
    double temperature_;
    double humidity_;
};

}  // namespace ros_sim
