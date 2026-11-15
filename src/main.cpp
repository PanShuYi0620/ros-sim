#include "ros/logger.hpp"

int main() {
    auto& logger = ros_sim::Logger::instance();

    logger.info("程序启动");
    logger.warn("这是一条警告");
    logger.error("这是一条错误");

    return 0;
}
