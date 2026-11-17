#include "ros/subscriber.hpp"

#include <utility>

#include "ros/exception.hpp"
#include "ros/logger.hpp"
#include "ros/ros.hpp"

namespace ros_sim {

namespace {

std::string validateTopic(std::string topic) {
    if (topic.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw TopicException("话题名不能为空");
    }
    return topic;
}

} // namespace

Subscriber::Subscriber(std::string name, std::string topic)
    : Node(std::move(name)), topic_(validateTopic(std::move(topic))) {}

void Subscriber::init() {
    if (initialized_) {
        throw NodeException(name(), "重复初始化");
    }

    Logger::instance().info("[Node:" + name() + "] 订阅者初始化完成 topic=" + topic_);
    ROS::instance().registerSubscriber(std::static_pointer_cast<Subscriber>(shared_from_this()));
    initialized_ = true;
}

void Subscriber::shutdown() {
    if (!initialized_) {
        Logger::instance().warn("[Node:" + name() + "] 未初始化就调用关闭");
        return;
    }
    Logger::instance().info("[Node:" + name() + "] 订阅者已关闭");
    initialized_ = false;
}

void Subscriber::receive(const Message& message) {
    // toString() 是虚函数：如果传进来的是 SensorMessage，会打印出它的额外字段
    Logger::instance().info("[Node:" + name() + "] 收到消息: " + message.toString());

    inbox_.push_back(message.toString());
    if (inbox_.size() > 100) {
        inbox_.erase(inbox_.begin());
    }
}

void Subscriber::restoreInbox(std::vector<std::string> messages) {
    inbox_ = std::move(messages);
}

} // namespace ros_sim
