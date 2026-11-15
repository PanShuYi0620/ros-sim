#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ros/message.hpp"
#include "ros/publisher.hpp"
#include "ros/subscriber.hpp"

namespace ros_sim {

// 通信核心：单例，维护“话题→发布者 / 话题→订阅者”两张表
class ROS final {
public:
    static ROS& instance();

    ROS(const ROS&) = delete;
    ROS& operator=(const ROS&) = delete;
    ROS(ROS&&) = delete;
    ROS& operator=(ROS&&) = delete;

    void registerPublisher(const std::shared_ptr<Publisher>& publisher);
    void registerSubscriber(const std::shared_ptr<Subscriber>& subscriber);

    void publish(const std::string& topic, const Message& message);

    bool hasSubscriber(const std::string& topic) const;
    std::size_t publisherCount(const std::string& topic) const;
    std::size_t subscriberCount(const std::string& topic) const;

    void shutdownAll();

private:
    ROS() = default;
    ~ROS() = default;

    void ensureNameAvailable(const std::string& name) const;

    std::unordered_map<std::string, std::vector<std::shared_ptr<Publisher>>> publishers_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Subscriber>>> subscribers_;
    std::unordered_set<std::string> nodeNames_;
};

}  // namespace ros_sim
