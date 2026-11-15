#include "ros/node.hpp"

#include <utility>

#include "ros/exception.hpp"

namespace ros_sim {

std::string Node::validateName(std::string name) {
    const auto first = name.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) {
        throw NodeException(name, "节点名不能为空或全为空白");
    }
    return name;
}

Node::Node(std::string name) : name_(validateName(std::move(name))) {}

}  // namespace ros_sim
