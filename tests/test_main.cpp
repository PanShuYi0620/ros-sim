#include <iostream>
#include <memory>
#include <string>

#include "ros/exception.hpp"
#include "ros/message.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/sensor_message.hpp"
#include "ros/subscriber.hpp"

namespace {

int g_passed = 0;
int g_failed = 0;

void check(bool ok, const std::string& name) {
    if (ok) {
        ++g_passed;
        std::cout << "[PASS] " << name << "\n";
    } else {
        ++g_failed;
        std::cout << "[FAIL] " << name << "\n";
    }
}

// 执行 func，如果抛出指定类型的异常就返回 true
template <typename ExceptionType, typename Func>
bool throwsAs(Func&& func) {
    try {
        func();
    } catch (const ExceptionType&) {
        return true;
    } catch (...) {
        return false;
    }
    return false;
}

void testMessage() {
    check(throwsAs<ros_sim::MessageException>([] { ros_sim::Message m(""); }),
          "空消息内容抛 MessageException");

    ros_sim::Message ok("hello");
    check(ok.content() == "hello", "消息内容读取正确");
    check(ok.timestamp().size() == 19, "时间戳长度为 19（YYYY-MM-DD HH:MM:SS）");
}

void testNodeName() {
    check(throwsAs<ros_sim::NodeException>([] { ros_sim::Publisher p("", "/t"); }),
          "空节点名抛 NodeException");
    check(throwsAs<ros_sim::NodeException>([] { ros_sim::Publisher p("   ", "/t"); }),
          "全空白节点名抛 NodeException");
    check(throwsAs<ros_sim::TopicException>([] { ros_sim::Publisher p("p1", "  "); }),
          "空话题名抛 TopicException");
}

void testDuplicatedName() {
    auto first = std::make_shared<ros_sim::Publisher>("dup_node", "/dup1");
    first->init();

    const bool caught = throwsAs<ros_sim::NodeException>([&] {
        auto second = std::make_shared<ros_sim::Publisher>("dup_node", "/dup2");
        second->init();
    });
    check(caught, "重复节点名抛 NodeException");

    ros_sim::ROS::instance().shutdownAll();
}

void testTopicNotExist() {
    auto publisher = std::make_shared<ros_sim::Publisher>("pub_solo", "/nobody");
    publisher->init();

    check(throwsAs<ros_sim::TopicException>([&] { publisher->publish("x"); }),
          "话题下没有订阅者时抛 TopicException");

    ros_sim::ROS::instance().shutdownAll();
}

void testNormalFlow() {
    auto publisher = std::make_shared<ros_sim::Publisher>("pub_ok", "/ok");
    auto subscriber = std::make_shared<ros_sim::Subscriber>("sub_ok", "/ok");
    publisher->init();
    subscriber->init();

    check(ros_sim::ROS::instance().subscriberCount("/ok") == 1, "订阅者注册数量为 1");
    check(!throwsAs<std::exception>([&] { publisher->publish("hello"); }),
          "正常发布不抛异常");

    ros_sim::ROS::instance().shutdownAll();
    check(ros_sim::ROS::instance().subscriberCount("/ok") == 0, "统一关闭后注册表清空");
}

void testSensorMessage() {
    ros_sim::SensorMessage msg("读数", 25.3, 60.0);
    check(msg.temperature() == 25.3, "SensorMessage 温度字段正确");
    check(msg.humidity() == 60.0, "SensorMessage 湿度字段正确");
    check(msg.toString().find("温度=25.3") != std::string::npos, "toString 包含温度");
}

}  // namespace

int main() {
    testMessage();
    testNodeName();
    testDuplicatedName();
    testTopicNotExist();
    testNormalFlow();
    testSensorMessage();

    std::cout << "\n通过: " << g_passed << "，失败: " << g_failed << "\n";
    return g_failed == 0 ? 0 : 1;
}
