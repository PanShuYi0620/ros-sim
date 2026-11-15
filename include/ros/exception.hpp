#pragma once
//标注该文件只能使用一次,防止被多次使用而导致系统报错

#include <stdexcept>
#include <string>

namespace ros_sim {

// 所有异常的基类，继承标准库的 runtime_error
class RosException : public std::runtime_error {
public:
    explicit RosException(const std::string& message)
        : std::runtime_error(message) {}
};

// 节点问题：空名称、重名、重复初始化
class NodeException : public RosException {
public:
    NodeException(const std::string& nodeName, const std::string& message)
        : RosException("[Node:" + nodeName + "] " + message) {}
};

// 话题问题：空话题、话题不存在
class TopicException : public RosException {
public:
    explicit TopicException(const std::string& message)
        : RosException("[Topic] " + message) {}
};

// 消息问题：空内容
class MessageException : public RosException {
public:
    explicit MessageException(const std::string& message)
        : RosException("[Message] " + message) {}
};

}  // namespace ros_sim
//总体上上面几个类，一个基类，3个子类，分别存储节点的异常问题，话题问题和消息问题，方便了后续统计