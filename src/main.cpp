#include <memory>

#include "ros/logger.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/subscriber.hpp"

int main() {
    auto& logger = ros_sim::Logger::instance();
    logger.info("========== 系统启动 ==========");

    auto publisher = std::make_shared<ros_sim::Publisher>("pub_temp", "/room/temp");
    auto display = std::make_shared<ros_sim::Subscriber>("sub_display", "/room/temp");
    auto app = std::make_shared<ros_sim::Subscriber>("sub_app", "/room/temp");

    publisher->init();
    display->init();
    app->init();

    publisher->publish("25.3");
    publisher->publish("25.8");

    ros_sim::ROS::instance().shutdownAll();
    logger.info("========== 系统关闭 ==========");

    return 0;
}
