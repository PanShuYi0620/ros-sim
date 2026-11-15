#include "ros/ros.hpp"

#include <exception>

#include "ros/exception.hpp"
#include "ros/logger.hpp"

namespace ros_sim {

ROS& ROS::instance() {
    static ROS instance;
    return instance;
}

void ROS::ensureNameAvailable(const std::string& name) const {
    if (nodeNames_.count(name) != 0) {
        throw NodeException(name, "节点名重复注册");
    }
}

void ROS::registerPublisher(const std::shared_ptr<Publisher>& publisher) {
    if (!publisher) {
        throw NodeException("<null>", "不能注册空指针发布者");
    }
    ensureNameAvailable(publisher->name());
    publishers_[publisher->topic()].push_back(publisher);
    nodeNames_.insert(publisher->name());
    Logger::instance().info("[Node:" + publisher->name() +
                            "] 已注册为发布者 topic=" + publisher->topic());
}

void ROS::registerSubscriber(const std::shared_ptr<Subscriber>& subscriber) {
    if (!subscriber) {
        throw NodeException("<null>", "不能注册空指针订阅者");
    }
    ensureNameAvailable(subscriber->name());
    subscribers_[subscriber->topic()].push_back(subscriber);
    nodeNames_.insert(subscriber->name());
    Logger::instance().info("[Node:" + subscriber->name() +
                            "] 已注册为订阅者 topic=" + subscriber->topic());
}

void ROS::publish(const std::string& topic, const Message& message) {
    const auto it = subscribers_.find(topic);
    if (it == subscribers_.end() || it->second.empty()) {
        throw TopicException("话题不存在或没有订阅者: " + topic);
    }

    for (const auto& subscriber : it->second) {
        try {
            subscriber->receive(message);
        } catch (const std::exception& e) {
            Logger::instance().error("[Topic:" + topic + "] 转发失败: " + e.what());
        }
    }
}

bool ROS::hasSubscriber(const std::string& topic) const {
    const auto it = subscribers_.find(topic);
    return it != subscribers_.end() && !it->second.empty();
}

std::size_t ROS::publisherCount(const std::string& topic) const {
    const auto it = publishers_.find(topic);
    return it == publishers_.end() ? 0 : it->second.size();
}

std::size_t ROS::subscriberCount(const std::string& topic) const {
    const auto it = subscribers_.find(topic);
    return it == subscribers_.end() ? 0 : it->second.size();
}

void ROS::shutdownAll() {
    for (auto& entry : publishers_) {
        for (auto& publisher : entry.second) {
            publisher->shutdown();
        }
    }
    for (auto& entry : subscribers_) {
        for (auto& subscriber : entry.second) {
            subscriber->shutdown();
        }
    }
    publishers_.clear();
    subscribers_.clear();
    nodeNames_.clear();
}

}  // namespace ros_sim
