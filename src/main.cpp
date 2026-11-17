#include <memory>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ros/exception.hpp"
#include "ros/logger.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/sensor_message.hpp"
#include "ros/subscriber.hpp"

namespace {

// 正常业务流程：初始化 -> 收发 -> 关闭
void run_demo() {
    auto& logger = ros_sim::Logger::instance();
    logger.info("========== 系统启动 ==========");

    auto temp_publisher = std::make_shared<ros_sim::Publisher>("pub_temp", "/room/temp");
    auto temp_display = std::make_shared<ros_sim::Subscriber>("sub_temp_display", "/room/temp");
    auto temp_app = std::make_shared<ros_sim::Subscriber>("sub_temp_app", "/room/temp");

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

    // 自定义消息：除了内容，还带温度和湿度
    ros_sim::SensorMessage sensor("传感器读数", 25.3, 60.0);
    humid_publisher->publish(sensor);

    ros_sim::ROS::instance().shutdownAll();
    logger.info("========== 系统关闭 ==========");
}

// 测试用：故意发布到没有订阅者的话题，验证异常被捕获
void test_topic_exception() {
    auto bad_publisher = std::make_shared<ros_sim::Publisher>("pub_bad", "/room/none");
    bad_publisher->init();
    bad_publisher->publish("这条一定会失败,用来测试程序面对错误是否会崩溃");
}

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // 让 Windows 控制台按 UTF-8 显示中文，这样子运行就不会出现乱码了
#endif
    // 这样就代表在windows上能运行，linux上面运行不了

    auto& logger = ros_sim::Logger::instance();

    try {
        run_demo();
        test_topic_exception();
    } catch (const ros_sim::NodeException& e) {
        logger.error(e.what());
        return 1;
        // 程序遇到了错误，但是受控退出，操作系统和脚本能根据这个数字判断程序有没有出错
    } catch (const ros_sim::TopicException& e) {
        logger.error(e.what());
        return 1;
        // 程序遇到了错误，但是受控退出，操作系统和脚本能根据这个数字判断程序有没有出错
    } catch (const ros_sim::MessageException& e) {
        logger.error(e.what());
        return 1;
        // 程序遇到了错误，但是受控退出，操作系统和脚本能根据这个数字判断程序有没有出错
    } catch (const std::exception& e) {
        logger.error(std::string("未知异常: ") + e.what());
        return 1;
        // 程序遇到了错误，但是受控退出，操作系统和脚本能根据这个数字判断程序有没有出错
    }

    return 0;
}
