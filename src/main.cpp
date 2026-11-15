#include "ros/exception.hpp"
#include "ros/logger.hpp"
#include "ros/message.hpp"

int main() {
    auto& logger = ros_sim::Logger::instance();

    try {
        ros_sim::Message msg("温度 25.3");
        logger.info("创建消息: " + msg.toString());
    } catch (const ros_sim::MessageException& e) {
        logger.error(e.what());
    }

    try {
        ros_sim::Message empty("");
        logger.info("不该走到这里");
    } catch (const ros_sim::MessageException& e) {
        logger.error("预期异常: " + std::string(e.what()));
    }

    return 0;
}
