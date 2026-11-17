#pragma once

#include <string>
#include <vector>

#include "ros/message.hpp"
#include "ros/node.hpp"

namespace ros_sim {

// 订阅者：被动接收指定话题的消息
class Subscriber final : public Node {
  public:
    Subscriber(std::string name, std::string topic);

    void init() override;
    void shutdown() override;

    const std::string& topic() const noexcept {
        return topic_;
    }

    void receive(const Message& message);

    // 收到的消息（最多保留最近 100 条），供交互式控制台查看
    const std::vector<std::string>& inbox() const noexcept {
        return inbox_;
    }

    // 从持久化状态恢复消息箱
    void restoreInbox(std::vector<std::string> messages);

  private:
    std::string topic_;
    bool initialized_{false};
    std::vector<std::string> inbox_;
};

} // namespace ros_sim
