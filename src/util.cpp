#include "ros/util.hpp"

#include <ctime>
#include <iomanip>
#include <sstream>

#include "ros/exception.hpp"

namespace ros_sim {

std::string nowTimestamp() {
    std::time_t now = std::time(nullptr);
    std::tm* local = std::localtime(&now);

    std::ostringstream oss;
    oss << std::put_time(local, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

std::string validateTopicName(std::string topic) {
    if (topic.find_first_not_of(" \t\r\n") == std::string::npos) {
        throw TopicException("话题名不能为空");
    }
    return topic;
}

} // namespace ros_sim
