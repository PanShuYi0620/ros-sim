#include <memory>

#include "ros/logger.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/subscriber.hpp"

int main() {
    auto& logger = ros_sim::Logger::instance();
    logger.info("========== 系统启动 ==========");

    auto temp_publisher = std::make_shared<ros_sim::Publisher>("pub_temp", "/room/temp");
    auto temp_display = std::make_shared<ros_sim::Subscriber>("sub_temp_display", "/room/temp");
    auto temp_app = std::make_shared<ros_sim::Subscriber>("sub_temp_app", "/room/temp");
    //温度topic
    auto humid_publisher = std::make_shared<ros_sim::Publisher>("pub_humid", "/room/humid");
    auto humid_display = std::make_shared<ros_sim::Subscriber>("sub_humid_display", "/room/humid");
    auto humid_app = std::make_shared<ros_sim::Subscriber>("sub_humid_app", "/room/humid");
    temp_publisher->init();
    temp_display->init();
    temp_app->init();
    humid_publisher->init();
    humid_display->init();
    humid_app->init();

    temp_publisher->publish("25.3");
    temp_publisher->publish("25.8");
    humid_publisher->publish("60%");

    ros_sim::ROS::instance().shutdownAll();
    logger.info("========== 系统关闭 ==========");

    return 0;
}
