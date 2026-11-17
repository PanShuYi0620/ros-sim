#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ros/exception.hpp"
#include "ros/logger.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/subscriber.hpp"

namespace {

// 交互式控制台：运行时创建节点、发布消息、查看订阅者收到的内容
class Console {
public:
    void run() {
        printHelp();

        std::string line;
        while (true) {
            std::cout << "\n> " << std::flush;
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line.empty()) {
                continue;
            }
            if (!handle(line)) {
                break;
            }
        }

        ros_sim::ROS::instance().shutdownAll();
        std::cout << "已退出\n";
    }

private:
    bool handle(const std::string& line) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "help") { printHelp(); return true; }
        if (command == "exit" || command == "quit") { return false; }
        if (command == "list") { list(); return true; }
        if (command == "pub") { createPublisher(iss); return true; }
        if (command == "sub") { createSubscriber(iss); return true; }
        if (command == "send") { send(iss); return true; }
        if (command == "recv") { recv(iss); return true; }

        std::cout << "未知命令: " << command << "，输入 help 查看帮助\n";
        return true;
    }

    void createPublisher(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: pub <名字> <话题>\n";
            return;
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(name, topic);
            publisher->init();
            publishers_[name] = publisher;
            std::cout << "发布者 " << name << " 已创建，话题 " << topic << "\n";
        } catch (const std::exception& e) {
            std::cout << "创建失败: " << e.what() << "\n";
        }
    }

    void createSubscriber(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: sub <名字> <话题>\n";
            return;
        }

        try {
            auto subscriber = std::make_shared<ros_sim::Subscriber>(name, topic);
            subscriber->init();
            subscribers_[name] = subscriber;
            std::cout << "订阅者 " << name << " 已创建，话题 " << topic << "\n";
        } catch (const std::exception& e) {
            std::cout << "创建失败: " << e.what() << "\n";
        }
    }

    void send(std::istringstream& iss) {
        std::string name;
        iss >> name;
        if (name.empty()) {
            std::cout << "用法: send <发布者名字> <内容>\n";
            return;
        }

        auto it = publishers_.find(name);
        if (it == publishers_.end()) {
            std::cout << "没有这个发布者: " << name << "\n";
            return;
        }

        std::string content;
        std::string word;
        while (iss >> word) {
            if (!content.empty()) {
                content += " ";
            }
            content += word;
        }
        if (content.empty()) {
            std::cout << "用法: send <发布者名字> <内容>\n";
            return;
        }

        try {
            it->second->publish(content);
        } catch (const std::exception& e) {
            std::cout << "发布失败: " << e.what() << "\n";
        }
    }

    void recv(std::istringstream& iss) {
        std::string name;
        iss >> name;

        auto it = subscribers_.find(name);
        if (it == subscribers_.end()) {
            std::cout << "没有这个订阅者: " << name << "\n";
            return;
        }

        const auto& inbox = it->second->inbox();
        if (inbox.empty()) {
            std::cout << "订阅者 " << name << " 还没有收到消息\n";
            return;
        }

        std::cout << "订阅者 " << name << " 收到 " << inbox.size() << " 条消息:\n";
        for (const auto& message : inbox) {
            std::cout << "  " << message << "\n";
        }
    }

    void list() const {
        std::cout << "发布者: " << publishers_.size() << " 个\n";
        for (const auto& entry : publishers_) {
            std::cout << "  " << entry.first << " -> " << entry.second->topic() << "\n";
        }

        std::cout << "订阅者: " << subscribers_.size() << " 个\n";
        for (const auto& entry : subscribers_) {
            std::cout << "  " << entry.first << " -> " << entry.second->topic()
                      << "（已收 " << entry.second->inbox().size() << " 条）\n";
        }
    }

    void printHelp() const {
        std::cout << "可用命令:\n"
                  << "  pub  <名字> <话题>      创建发布者\n"
                  << "  sub  <名字> <话题>      创建订阅者\n"
                  << "  send <发布者> <内容>    用指定发布者发消息\n"
                  << "  recv <订阅者>           查看该订阅者收到的消息\n"
                  << "  list                    查看当前所有节点\n"
                  << "  help                    显示帮助\n"
                  << "  exit                    关闭所有节点并退出\n";
    }

    std::unordered_map<std::string, std::shared_ptr<ros_sim::Publisher>> publishers_;
    std::unordered_map<std::string, std::shared_ptr<ros_sim::Subscriber>> subscribers_;
};

}  // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);   // 让 Windows 控制台按 UTF-8 显示中文
#endif

    Console console;
    console.run();
    return 0;
}
