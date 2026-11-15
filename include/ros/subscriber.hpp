#pragma once

#include <string>

#include "ros/message.hpp"
#include "ros/node.hpp"

namespace ros_sim {

// 订阅者：被动接收指定话题的消息
class Subscriber final : public Node {
public:
    Subscriber(std::string name, std::string topic);

    void init() override;
    void shutdown() override;

    const std::string& topic() const noexcept { return topic_; }

    void receive(const Message& message);

private:
    std::string topic_;
    bool initialized_{false};
};

}  // namespace ros_sim
