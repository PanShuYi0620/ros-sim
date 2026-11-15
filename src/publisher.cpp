#include "ros/publisher.hpp"

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

}  // namespace

Publisher::Publisher(std::string name, std::string topic)
    : Node(std::move(name)), topic_(validateTopic(std::move(topic))) {}

void Publisher::init() {
    if (initialized_) {
        throw NodeException(name(), "重复初始化");
    }

    Logger::instance().info("[Node:" + name() + "] 发布者初始化完成 topic=" + topic_);
    ROS::instance().registerPublisher(std::static_pointer_cast<Publisher>(shared_from_this()));
    initialized_ = true;
}

void Publisher::shutdown() {
    if (!initialized_) {
        Logger::instance().warn("[Node:" + name() + "] 未初始化就调用关闭");
        return;
    }
    Logger::instance().info("[Node:" + name() + "] 发布者已关闭");
    initialized_ = false;
}

void Publisher::publish(const std::string& sensorData) {
    publish(Message(sensorData));
}

void Publisher::publish(const Message& message) {
    if (!initialized_) {
        throw NodeException(name(), "节点未初始化，不能发布消息");
    }

    Logger::instance().info("[Node:" + name() + "] 发布消息 topic=" + topic_ +
                            " 内容=" + message.content());
    ROS::instance().publish(topic_, message);
}

}  // namespace ros_sim
