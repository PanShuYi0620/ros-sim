#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#endif

#include "ros/exception.hpp"
#include "ros/logger.hpp"
#include "ros/publisher.hpp"
#include "ros/ros.hpp"
#include "ros/subscriber.hpp"

namespace {

// 交互式控制台：
// 1. 先创建发布者（此时还没有话题）
// 2. 让某个发布者创建话题，或加入已存在的话题
// 3. 订阅者只能订阅已经创建的话题
// 4. 日志以旁白方式记录每一步
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
    struct TopicInfo {
        std::string creator;
        std::vector<std::string> publishers;
    };

    bool handle(const std::string& line) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "help") { printHelp(); return true; }
        if (command == "exit" || command == "quit") { return false; }
        if (command == "list") { list(); return true; }
        if (command == "pub") { createPublisher(iss); return true; }
        if (command == "topic") { assignTopic(iss); return true; }
        if (command == "join") { joinTopic(iss); return true; }
        if (command == "sub") { createSubscriber(iss); return true; }
        if (command == "send") { send(iss); return true; }
        if (command == "recv") { recv(iss); return true; }

        std::cout << "未知命令: " << command << "，输入 help 查看帮助\n";
        return true;
    }

    bool nameTaken(const std::string& name) const {
        return pending_publishers_.count(name) != 0 || active_publishers_.count(name) != 0 ||
               subscribers_.count(name) != 0;
    }

    // pub <名字>：只创建发布者，不分配话题
    void createPublisher(std::istringstream& iss) {
        std::string name;
        if (!(iss >> name)) {
            std::cout << "用法: pub <名字>\n";
            return;
        }
        if (nameTaken(name)) {
            std::cout << "名字 " << name << " 已被占用\n";
            return;
        }

        pending_publishers_.insert(name);
        narrative("[Node:" + name + "] 发布者已创建（尚未分配话题）");
    }

    // topic <发布者> <话题>：由该发布者创建话题
    void assignTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: topic <发布者> <话题>\n";
            return;
        }
        if (pending_publishers_.count(name) == 0) {
            if (active_publishers_.count(name) != 0) {
                std::cout << "发布者 " << name << " 已经有话题了\n";
            } else {
                std::cout << "没有这个发布者: " << name << "\n";
            }
            return;
        }
        if (topics_.count(topic) != 0) {
            std::cout << "话题 " << topic << " 已存在，请用 join 加入\n";
            return;
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(name, topic);
            publisher->init();
            active_publishers_[name] = publisher;
            pending_publishers_.erase(name);
            topics_[topic] = TopicInfo{name, {name}};
            narrative("[Topic:" + topic + "] 话题由 " + name + " 创建");
        } catch (const std::exception& e) {
            std::cout << "创建话题失败: " << e.what() << "\n";
        }
    }

    // join <发布者> <话题>：让已有发布者加入已存在的话题
    void joinTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: join <发布者> <话题>\n";
            return;
        }
        if (pending_publishers_.count(name) == 0) {
            std::cout << "没有可用的发布者: " << name << "\n";
            return;
        }
        if (topics_.count(topic) == 0) {
            std::cout << "话题 " << topic << " 不存在\n";
            return;
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(name, topic);
            publisher->init();
            active_publishers_[name] = publisher;
            pending_publishers_.erase(name);
            topics_[topic].publishers.push_back(name);
            narrative("[Topic:" + topic + "] 发布者 " + name + " 加入了话题");
        } catch (const std::exception& e) {
            std::cout << "加入话题失败: " << e.what() << "\n";
        }
    }

    // sub <名字> <话题>：话题必须已存在
    void createSubscriber(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: sub <名字> <话题>\n";
            return;
        }
        if (topics_.count(topic) == 0) {
            std::cout << "话题 " << topic << " 还没有创建，不能订阅\n";
            return;
        }
        if (nameTaken(name)) {
            std::cout << "名字 " << name << " 已被占用\n";
            return;
        }

        try {
            auto subscriber = std::make_shared<ros_sim::Subscriber>(name, topic);
            subscriber->init();
            subscribers_[name] = subscriber;
            narrative("[Topic:" + topic + "] 订阅者 " + name + " 订阅了话题");
        } catch (const std::exception& e) {
            std::cout << "创建订阅者失败: " << e.what() << "\n";
        }
    }

    // send <发布者> <内容>
    void send(std::istringstream& iss) {
        std::string name;
        iss >> name;
        if (name.empty()) {
            std::cout << "用法: send <发布者名字> <内容>\n";
            return;
        }

        auto it = active_publishers_.find(name);
        if (it == active_publishers_.end()) {
            if (pending_publishers_.count(name) != 0) {
                std::cout << "发布者 " << name << " 还没有话题，不能发布\n";
            } else {
                std::cout << "没有这个发布者: " << name << "\n";
            }
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

        const std::string topic = it->second->topic();
        try {
            it->second->publish(content);
            narrative("[Topic:" + topic + "] " + name + " 发布完成，共 " +
                      std::to_string(ros_sim::ROS::instance().subscriberCount(topic)) +
                      " 个订阅者收到");
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
        std::cout << "话题: " << topics_.size() << " 个\n";
        for (const auto& entry : topics_) {
            std::cout << "  " << entry.first << "  创建者: " << entry.second.creator
                      << "  发布者: " << entry.second.publishers.size()
                      << "  订阅者: " << ros_sim::ROS::instance().subscriberCount(entry.first)
                      << "\n";
        }

        std::cout << "发布者: " << (pending_publishers_.size() + active_publishers_.size())
                  << " 个\n";
        for (const auto& name : pending_publishers_) {
            std::cout << "  " << name << " -> (还没有话题)\n";
        }
        for (const auto& entry : active_publishers_) {
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
                  << "  pub   <名字>            创建发布者（此时还没有话题）\n"
                  << "  topic <发布者> <话题>   让该发布者创建话题\n"
                  << "  join  <发布者> <话题>   让该发布者加入已存在的话题\n"
                  << "  sub   <名字> <话题>     创建订阅者（话题必须已存在）\n"
                  << "  send  <发布者> <内容>   用该发布者发消息\n"
                  << "  recv  <订阅者>          查看该订阅者收到的消息\n"
                  << "  list                    查看话题与节点\n"
                  << "  help                    显示帮助\n"
                  << "  exit                    关闭所有节点并退出\n";
    }

    static void narrative(const std::string& text) {
        ros_sim::Logger::instance().info(text);
    }

    std::unordered_map<std::string, std::shared_ptr<ros_sim::Publisher>> active_publishers_;
    std::unordered_map<std::string, std::shared_ptr<ros_sim::Subscriber>> subscribers_;
    std::set<std::string> pending_publishers_;
    std::map<std::string, TopicInfo> topics_;
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
