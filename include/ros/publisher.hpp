#pragma once

#include <string>

#include "ros/message.hpp"
#include "ros/node.hpp"

namespace ros_sim {

// 发布者：往指定话题发消息
class Publisher final : public Node {
  public:
    Publisher(std::string name, std::string topic);

    void init() override;
    void shutdown() override;

    const std::string& topic() const noexcept {
        return topic_;
    }

    void publish(const std::string& sensorData);
    void publish(const Message& message);

  private:
    std::string topic_;
    bool initialized_{false};
};

} // namespace ros_sim
