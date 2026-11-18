#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <tuple>
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

namespace color {
// Reset 只重置前景色和加粗，保留背景色
constexpr const char* Reset = "\033[39m\033[22m";
constexpr const char* Bold = "\033[1m";
constexpr const char* Dim = "\033[90m";    // 灰
constexpr const char* Green = "\033[92m";  // 亮绿
constexpr const char* Cyan = "\033[96m";   // 亮青
constexpr const char* Yellow = "\033[93m"; // 亮黄
constexpr const char* Red = "\033[91m";    // 亮红
// 深蓝底黑字；要改背景：40 黑 / 44 蓝 / 48;5;17 深蓝
constexpr const char* Background = "\033[48;5;17m";
// 纯黑文字；如果终端不认 256 色，改成 "\033[30m"
constexpr const char* BrightWhite = "\033[38;5;16m";
} // namespace color

void printOk(const std::string& text) {
    std::cout << color::Green << "✔ " << color::Reset << text << "\n";
}

void printErr(const std::string& text) {
    std::cout << color::Red << "✘ " << color::Reset << text << "\n";
}

void printUsage(const std::string& text) {
    std::cout << color::Dim << "用法: " << text << color::Reset << "\n";
}

// 交互式控制台
// 发布者：先创建，再绑定一个或多个话题（topic 创建 / join 加入）
// 订阅者：先创建，再通过 listen 绑定到已存在的话题
// 发送时必须指明「哪个发布者」+「哪个话题」，避免多话题时歧义
// 控制台只显示交互内容，系统日志写入 logs/ros_sim.log
class Console {
  public:
    void run() {
        ros_sim::Logger::instance().setConsoleEnabled(false);
        loadState();

        // 背景 + 亮白前景，并清屏
        std::cout << color::Background << color::BrightWhite << "\033[2J\033[H";

        std::cout << color::Cyan << color::Bold << "============================================\n"
                  << "        ros_sim 交互式控制台\n"
                  << "   输入 help 查看命令，exit 保存并退出\n"
                  << "============================================" << color::Reset << "\n";
        printHelp();

        std::string line;
        while (true) {
            std::cout << "\n" << color::Cyan << color::Bold << "> " << color::Reset << std::flush;
            if (!std::getline(std::cin, line)) {
                break;
            }
            if (line.empty()) {
                continue;
            }
            history_.push_back(line);
            if (history_.size() > 20) {
                history_.erase(history_.begin());
            }
            const bool keepGoing = handle(line);
            saveState(); // 每条命令后都保存，直接关窗口也不会丢
            if (!keepGoing) {
                break;
            }
        }

        ros_sim::ROS::instance().shutdownAll();
        printOk("已退出（状态已保存，下次启动会恢复）");
        std::cout << "\033[0m"; // 退出前恢复正常配色
    }

  private:
    struct TopicInfo {
        std::string creator;
        std::vector<std::string> publishers;
        std::set<std::string> removed_publishers;
        std::size_t message_count{0};
    };

    // 一个节点名可以对应多个话题，所以是 名字 -> (话题 -> 发布者对象)
    using PublisherMap = std::map<std::string, std::shared_ptr<ros_sim::Publisher>>;

    bool handle(const std::string& line) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "help") {
            printHelp();
            return true;
        }
        if (command == "exit" || command == "quit") {
            return false;
        }
        if (command == "list") {
            list();
            return true;
        }
        if (command == "pub") {
            createPublisher(iss);
            return true;
        }
        if (command == "sub") {
            createSubscriber(iss);
            return true;
        }
        if (command == "topic") {
            assignTopic(iss);
            return true;
        }
        if (command == "join") {
            joinTopic(iss);
            return true;
        }
        if (command == "listen") {
            bindTopic(iss);
            return true;
        }
        if (command == "send") {
            send(iss);
            return true;
        }
        if (command == "recv") {
            recv(iss);
            return true;
        }
        if (command == "del") {
            removeNode(iss);
            return true;
        }
        if (command == "info") {
            showTopic(iss);
            return true;
        }
        if (command == "clearlog") {
            if (ros_sim::Logger::instance().clearLogFile()) {
                printOk("日志已清空（logs/ros_sim.log）");
            } else {
                printErr("日志清空失败");
            }
            return true;
        }
        if (command == "clear") {
            clearAll();
            return true;
        }
        if (command == "history") {
            showHistory();
            return true;
        }

        printErr("未知命令: " + command + "，输入 help 查看帮助");
        return true;
    }

    static std::string internalName(const std::string& node, const std::string& topic) {
        return node + "@" + topic;
    }

    bool nodeExists(const std::string& name) const {
        return pending_publishers_.count(name) != 0 || active_publishers_.count(name) != 0 ||
               pending_subscribers_.count(name) != 0 || subscribers_.count(name) != 0;
    }

    bool publisherBound(const std::string& node, const std::string& topic) const {
        auto it = active_publishers_.find(node);
        return it != active_publishers_.end() && it->second.count(topic) != 0;
    }

    // pub <名字>
    void createPublisher(std::istringstream& iss) {
        std::string name;
        if (!(iss >> name)) {
            printUsage("pub <名字>");
            return;
        }
        if (nodeExists(name)) {
            printErr("名字 " + name + " 已被占用");
            return;
        }

        pending_publishers_.insert(name);
        printOk("发布者 " + name + " 已创建（尚未绑定话题）");
        narrative("[Node:" + name + "] 发布者已创建（尚未绑定话题）");
    }

    // sub <名字>
    void createSubscriber(std::istringstream& iss) {
        std::string name;
        if (!(iss >> name)) {
            printUsage("sub <名字>");
            return;
        }
        if (nodeExists(name)) {
            printErr("名字 " + name + " 已被占用");
            return;
        }

        pending_subscribers_.insert(name);
        printOk("订阅者 " + name + " 已创建（尚未订阅话题）");
        narrative("[Node:" + name + "] 订阅者已创建（尚未订阅话题）");
    }

    // topic <发布者> <话题>：创建新话题并绑定
    void assignTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            printUsage("topic <发布者> <话题>");
            return;
        }
        if (!nodeExists(name)) {
            printErr("没有这个发布者: " + name);
            return;
        }
        if (topics_.count(topic) != 0) {
            printErr("话题 " + topic + " 已存在，请用 join 加入");
            return;
        }
        if (publisherBound(name, topic)) {
            printErr("发布者 " + name + " 已经绑定话题 " + topic);
            return;
        }

        if (deleted_topics_.count(topic) != 0) {
            deleted_topics_.erase(topic);
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(internalName(name, topic), topic);
            publisher->init();
            active_publishers_[name][topic] = publisher;
            pending_publishers_.erase(name);
            topics_[topic] = TopicInfo{name, {name}, {}, 0};
            printOk("话题 " + topic + " 已创建，创建者 " + name);
            narrative("[Topic:" + topic + "] 话题由 " + name + " 创建");
        } catch (const std::exception& e) {
            printErr(std::string("创建话题失败: ") + e.what());
        }
    }

    // join <发布者> <话题>：把已有发布者绑定到已有话题
    void joinTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            printUsage("join <发布者> <话题>");
            return;
        }
        if (!nodeExists(name)) {
            printErr("没有这个发布者: " + name);
            return;
        }
        if (deleted_topics_.count(topic) != 0) {
            printErr("话题 " + topic + " 已被删除，请先重新创建");
            return;
        }
        if (topics_.count(topic) == 0) {
            printErr("话题 " + topic + " 不存在");
            return;
        }
        if (publisherBound(name, topic)) {
            printErr("发布者 " + name + " 已经绑定话题 " + topic);
            return;
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(internalName(name, topic), topic);
            publisher->init();
            active_publishers_[name][topic] = publisher;
            pending_publishers_.erase(name);
            topics_[topic].publishers.push_back(name);
            topics_[topic].removed_publishers.erase(name);
            printOk("发布者 " + name + " 已绑定话题 " + topic);
            narrative("[Topic:" + topic + "] 发布者 " + name + " 加入了话题");
        } catch (const std::exception& e) {
            printErr(std::string("绑定话题失败: ") + e.what());
        }
    }

    // listen <订阅者> <话题>
    void bindTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            printUsage("listen <订阅者> <话题>");
            return;
        }
        if (pending_subscribers_.count(name) == 0) {
            if (subscribers_.count(name) != 0) {
                printErr("订阅者 " + name + " 已经订阅了话题");
            } else {
                printErr("没有这个订阅者: " + name);
            }
            return;
        }
        if (topics_.count(topic) == 0) {
            if (deleted_topics_.count(topic) != 0) {
                printErr("话题 " + topic + " 已被删除，请先重新创建");
            } else {
                printErr("话题 " + topic + " 还没有创建，不能订阅");
            }
            return;
        }

        try {
            auto subscriber = std::make_shared<ros_sim::Subscriber>(name, topic);
            subscriber->init();
            subscribers_[name] = subscriber;
            pending_subscribers_.erase(name);
            printOk("订阅者 " + name + " 已订阅话题 " + topic);
            narrative("[Topic:" + topic + "] 订阅者 " + name + " 订阅了话题");
        } catch (const std::exception& e) {
            printErr(std::string("订阅失败: ") + e.what());
        }
    }

    // send <发布者> <话题> <内容>
    void send(std::istringstream& iss) {
        std::string name;
        std::string topic;
        iss >> name >> topic;
        if (name.empty() || topic.empty()) {
            printUsage("send <发布者名字> <话题> <内容>");
            return;
        }

        auto nodeIt = active_publishers_.find(name);
        if (nodeIt == active_publishers_.end()) {
            if (pending_publishers_.count(name) != 0) {
                printErr("发布者 " + name + " 还没有绑定话题，不能发布");
            } else {
                printErr("没有这个发布者: " + name);
            }
            return;
        }

        auto topicIt = nodeIt->second.find(topic);
        if (topicIt == nodeIt->second.end()) {
            printErr("发布者 " + name + " 没有绑定话题 " + topic);
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
            printUsage("send <发布者名字> <话题> <内容>");
            return;
        }

        try {
            topicIt->second->publish(content);
            auto infoIt = topics_.find(topic);
            if (infoIt != topics_.end()) {
                ++infoIt->second.message_count;
            }
            printOk("已发送到 " + topic + "，" +
                    std::to_string(ros_sim::ROS::instance().subscriberCount(topic)) +
                    " 个订阅者收到");
            narrative("[Topic:" + topic + "] " + name + " 发布完成，共 " +
                      std::to_string(ros_sim::ROS::instance().subscriberCount(topic)) +
                      " 个订阅者收到");
        } catch (const std::exception& e) {
            printErr(std::string("发布失败: ") + e.what());
        }
    }

    void recv(std::istringstream& iss) {
        std::string name;
        if (!(iss >> name)) {
            printUsage("recv <订阅者>");
            return;
        }

        if (pending_subscribers_.count(name) != 0) {
            printErr("订阅者 " + name + " 还没有订阅话题");
            return;
        }

        auto it = subscribers_.find(name);
        if (it == subscribers_.end()) {
            printErr("没有这个订阅者: " + name);
            return;
        }

        const auto& inbox = it->second->inbox();
        if (inbox.empty()) {
            printErr("订阅者 " + name + " 还没有收到消息");
            return;
        }

        std::cout << color::Cyan << "订阅者 " << name << " 收到 " << inbox.size()
                  << " 条消息:" << color::Reset << "\n";
        for (const auto& message : inbox) {
            std::cout << "  " << message << "\n";
        }
    }

    // del pub|sub|topic <名字>
    void removeNode(std::istringstream& iss) {
        std::string type;
        std::string name;
        if (!(iss >> type >> name)) {
            printUsage("del pub|sub|topic <名字>");
            return;
        }

        if (type == "pub") {
            std::string topic;
            iss >> topic; // 可选：del pub p1 /t 只解绑一个话题
            removePublisher(name, topic);
        } else if (type == "sub") {
            removeSubscriber(name);
        } else if (type == "topic") {
            removeTopic(name);
        } else {
            printUsage("del pub|sub|topic <名字>");
        }
    }

    void removePublisher(const std::string& name, const std::string& topic) {
        if (pending_publishers_.count(name) != 0) {
            pending_publishers_.erase(name);
            printOk("已删除发布者 " + name + "（它还没有绑定话题）");
            narrative("[Node:" + name + "] 发布者已删除");
            return;
        }

        auto it = active_publishers_.find(name);
        if (it == active_publishers_.end()) {
            printErr("没有这个发布者: " + name);
            return;
        }

        // 指定了话题：只解绑这一个话题
        if (!topic.empty()) {
            auto binding = it->second.find(topic);
            if (binding == it->second.end()) {
                printErr("发布者 " + name + " 没有绑定话题 " + topic);
                return;
            }

            try {
                binding->second->shutdown();
                ros_sim::ROS::instance().unregisterPublisher(binding->second);
            } catch (const std::exception&) {
            }
            it->second.erase(binding);

            auto infoIt = topics_.find(topic);
            if (infoIt != topics_.end()) {
                auto& list = infoIt->second.publishers;
                list.erase(std::remove(list.begin(), list.end(), name), list.end());
                infoIt->second.removed_publishers.insert(name);
            }

            if (it->second.empty()) {
                active_publishers_.erase(it);
                pending_publishers_.insert(name);
                printOk("已解绑发布者 " + name + " 与话题 " + topic + "（它暂时没有话题）");
            } else {
                printOk("已解绑发布者 " + name + " 与话题 " + topic + "（还有其他话题）");
            }
            narrative("[Node:" + name + "] 已解绑话题 " + topic);
            return;
        }

        std::vector<std::string> topics;
        for (auto& binding : it->second) {
            const std::string& current = binding.first;
            topics.push_back(current);
            try {
                binding.second->shutdown();
                ros_sim::ROS::instance().unregisterPublisher(binding.second);
            } catch (const std::exception&) {
            }

            auto infoIt = topics_.find(current);
            if (infoIt != topics_.end()) {
                auto& list = infoIt->second.publishers;
                list.erase(std::remove(list.begin(), list.end(), name), list.end());
                infoIt->second.removed_publishers.insert(name);
            }
        }
        active_publishers_.erase(it);

        std::string topicText;
        for (std::size_t i = 0; i < topics.size(); ++i) {
            topicText += (i == 0 ? "" : "、") + topics[i];
        }
        printOk("已删除发布者 " + name + "（话题 " + topicText +
                " 保留，它发过的消息仍在订阅者手里）");
        narrative("[Node:" + name + "] 发布者已删除，话题保留");
    }

    void removeSubscriber(const std::string& name) {
        if (pending_subscribers_.count(name) != 0) {
            pending_subscribers_.erase(name);
            printOk("已删除订阅者 " + name + "（它还没有订阅话题）");
            narrative("[Node:" + name + "] 订阅者已删除");
            return;
        }

        auto it = subscribers_.find(name);
        if (it == subscribers_.end()) {
            printErr("没有这个订阅者: " + name);
            return;
        }

        const std::string topic = it->second->topic();
        try {
            it->second->shutdown();
            ros_sim::ROS::instance().unregisterSubscriber(it->second);
            subscribers_.erase(it);
            printOk("已删除订阅者 " + name + "（已从话题 " + topic + " 退订）");
            narrative("[Node:" + name + "] 订阅者已删除，已从话题 " + topic + " 退订");
        } catch (const std::exception& e) {
            printErr(std::string("删除失败: ") + e.what());
        }
    }

    void removeTopic(const std::string& topic) {
        auto it = topics_.find(topic);
        if (it == topics_.end()) {
            if (deleted_topics_.count(topic) != 0) {
                printErr("话题 " + topic + " 已被删除");
            } else {
                printErr("没有这个话题: " + topic);
            }
            return;
        }
        if (!it->second.publishers.empty() ||
            ros_sim::ROS::instance().subscriberCount(topic) != 0) {
            printErr("话题 " + topic + " 还有发布者或订阅者，请先删除它们");
            return;
        }

        topics_.erase(it);
        deleted_topics_.insert(topic);
        printOk("已删除话题 " + topic);
        narrative("[Topic:" + topic + "] 话题已删除");
    }

    // clear：一键删除所有发布者、订阅者和话题
    void clearAll() {
        for (auto& node : active_publishers_) {
            for (auto& binding : node.second) {
                try {
                    binding.second->shutdown();
                    ros_sim::ROS::instance().unregisterPublisher(binding.second);
                } catch (const std::exception&) {
                }
            }
        }
        for (auto& entry : subscribers_) {
            try {
                entry.second->shutdown();
                ros_sim::ROS::instance().unregisterSubscriber(entry.second);
            } catch (const std::exception&) {
            }
        }

        active_publishers_.clear();
        subscribers_.clear();
        pending_publishers_.clear();
        pending_subscribers_.clear();
        topics_.clear();
        deleted_topics_.clear();

        printOk("已清空所有话题、发布者和订阅者");
        narrative("[System] 已清空全部节点与话题");
    }

    void showTopic(std::istringstream& iss) {
        std::string topic;
        if (!(iss >> topic)) {
            printUsage("info <话题>");
            return;
        }

        auto it = topics_.find(topic);
        if (it == topics_.end()) {
            if (deleted_topics_.count(topic) != 0) {
                printErr("话题 " + topic + " 已被删除");
            } else {
                printErr("没有这个话题: " + topic);
            }
            return;
        }

        const TopicInfo& info = it->second;
        std::cout << color::Cyan << color::Bold << "话题 " << topic << color::Reset << "\n";
        std::cout << "  创建者    " << info.creator << "\n";
        std::cout << "  发布者    " << info.publishers.size() << " 个";
        for (const auto& name : info.publishers) {
            std::cout << "  " << name << "(活动中)";
        }
        for (const auto& name : info.removed_publishers) {
            std::cout << "  " << name << "(已注销)";
        }
        std::cout << "\n";
        std::cout << "  订阅者    " << ros_sim::ROS::instance().subscriberCount(topic) << " 个\n";
        std::cout << "  消息记录  " << info.message_count << " 条\n";
    }

    void list() const {
        std::cout << color::Cyan << color::Bold << "话题 (" << topics_.size() << ")" << color::Reset
                  << "\n";
        for (const auto& entry : topics_) {
            std::cout << "  " << color::Bold << entry.first << color::Reset << "\n";
            std::cout << "    创建者 " << entry.second.creator << " · 发布者 "
                      << entry.second.publishers.size() << " · 订阅者 "
                      << ros_sim::ROS::instance().subscriberCount(entry.first) << " · 消息 "
                      << entry.second.message_count << " 条";
            if (!entry.second.removed_publishers.empty()) {
                std::cout << " · 已注销:";
                for (const auto& name : entry.second.removed_publishers) {
                    std::cout << " " << name;
                }
            }
            std::cout << "\n";
        }

        const std::size_t publisherCount = pending_publishers_.size() + active_publishers_.size();
        std::cout << color::Cyan << color::Bold << "发布者 (" << publisherCount << ")"
                  << color::Reset << "\n";
        for (const auto& name : pending_publishers_) {
            std::cout << "  " << name << " → (还没有绑定话题)\n";
        }
        for (const auto& node : active_publishers_) {
            std::cout << "  " << node.first << " → ";
            bool first = true;
            for (const auto& binding : node.second) {
                std::cout << (first ? "" : "、") << binding.first;
                first = false;
            }
            std::cout << "\n";
        }

        const std::size_t subscriberCount = pending_subscribers_.size() + subscribers_.size();
        std::cout << color::Cyan << color::Bold << "订阅者 (" << subscriberCount << ")"
                  << color::Reset << "\n";
        for (const auto& name : pending_subscribers_) {
            std::cout << "  " << name << " → (还没有订阅话题)\n";
        }
        for (const auto& entry : subscribers_) {
            std::cout << "  " << entry.first << " → " << entry.second->topic() << "（已收 "
                      << entry.second->inbox().size() << " 条）\n";
        }
    }

    static const char* stateFile() {
        return "console_state.txt";
    }

    void loadState() {
        std::ifstream in(stateFile());
        if (!in.is_open()) {
            return;
        }

        std::vector<std::tuple<std::string, std::string, std::size_t>> topicRecords;
        std::vector<std::pair<std::string, std::string>> removedRecords;
        std::vector<std::pair<std::string, std::string>> pubRecords;
        std::vector<std::string> pendingPubRecords;
        std::vector<std::pair<std::string, std::string>> subRecords;
        std::vector<std::string> pendingSubRecords;
        std::vector<std::string> deletedTopicRecords;
        std::map<std::string, std::vector<std::string>> inboxRecords;

        std::string line;
        while (std::getline(in, line)) {
            std::vector<std::string> fields;
            std::string part;
            std::istringstream iss(line);
            while (std::getline(iss, part, '\t')) {
                fields.push_back(part);
            }
            if (fields.empty()) {
                continue;
            }

            const std::string& tag = fields[0];
            if (tag == "TOPIC" && fields.size() >= 4) {
                try {
                    const auto count = static_cast<std::size_t>(std::stoul(fields[3]));
                    topicRecords.emplace_back(fields[1], fields[2], count);
                } catch (const std::exception&) {
                    // 状态文件里的坏记录直接跳过，不影响启动
                }
            } else if (tag == "REMOVED" && fields.size() >= 3) {
                removedRecords.emplace_back(fields[1], fields[2]);
            } else if (tag == "PUB" && fields.size() >= 3) {
                pubRecords.emplace_back(fields[1], fields[2]);
            } else if (tag == "PUBPEND" && fields.size() >= 2) {
                pendingPubRecords.push_back(fields[1]);
            } else if (tag == "SUB" && fields.size() >= 3) {
                subRecords.emplace_back(fields[1], fields[2]);
            } else if (tag == "SUBPEND" && fields.size() >= 2) {
                pendingSubRecords.push_back(fields[1]);
            } else if (tag == "DELTOPIC" && fields.size() >= 2) {
                deletedTopicRecords.push_back(fields[1]);
            } else if (tag == "MSG" && fields.size() >= 3) {
                inboxRecords[fields[1]].push_back(fields[2]);
            }
        }

        for (const auto& record : topicRecords) {
            TopicInfo info;
            info.creator = std::get<1>(record);
            info.message_count = std::get<2>(record);
            topics_[std::get<0>(record)] = info;
        }
        for (const auto& record : removedRecords) {
            auto it = topics_.find(record.first);
            if (it != topics_.end()) {
                it->second.removed_publishers.insert(record.second);
            }
        }
        for (const auto& record : pubRecords) {
            try {
                auto publisher = std::make_shared<ros_sim::Publisher>(
                    internalName(record.first, record.second), record.second);
                publisher->init();
                active_publishers_[record.first][record.second] = publisher;
                topics_[record.second].publishers.push_back(record.first);
            } catch (const std::exception&) {
                // 状态文件里的脏数据直接跳过，不影响启动
            }
        }
        for (const auto& name : pendingPubRecords) {
            pending_publishers_.insert(name);
        }
        for (const auto& record : subRecords) {
            try {
                auto subscriber =
                    std::make_shared<ros_sim::Subscriber>(record.first, record.second);
                subscriber->init();
                auto inboxIt = inboxRecords.find(record.first);
                if (inboxIt != inboxRecords.end()) {
                    subscriber->restoreInbox(inboxIt->second);
                }
                subscribers_[record.first] = subscriber;
            } catch (const std::exception&) {
            }
        }
        for (const auto& name : pendingSubRecords) {
            pending_subscribers_.insert(name);
        }
        for (const auto& name : deletedTopicRecords) {
            deleted_topics_.insert(name);
        }

        std::cout << "已恢复上次状态：话题 " << topics_.size() << " 个，发布者 "
                  << pending_publishers_.size() + active_publishers_.size() << " 个，订阅者 "
                  << pending_subscribers_.size() + subscribers_.size() << " 个\n";
    }

    void saveState() const {
        std::ofstream out(stateFile(), std::ios::trunc);
        if (!out.is_open()) {
            return;
        }

        for (const auto& entry : topics_) {
            out << "TOPIC\t" << entry.first << '\t' << entry.second.creator << '\t'
                << entry.second.message_count << '\n';
            for (const auto& name : entry.second.removed_publishers) {
                out << "REMOVED\t" << entry.first << '\t' << name << '\n';
            }
        }
        for (const auto& name : pending_publishers_) {
            out << "PUBPEND\t" << name << '\n';
        }
        for (const auto& node : active_publishers_) {
            for (const auto& binding : node.second) {
                out << "PUB\t" << node.first << '\t' << binding.first << '\n';
            }
        }
        for (const auto& name : pending_subscribers_) {
            out << "SUBPEND\t" << name << '\n';
        }
        for (const auto& entry : subscribers_) {
            out << "SUB\t" << entry.first << '\t' << entry.second->topic() << '\n';
            for (const auto& message : entry.second->inbox()) {
                std::string clean = message;
                std::replace(clean.begin(), clean.end(), '\t', ' ');
                out << "MSG\t" << entry.first << '\t' << clean << '\n';
            }
        }
        for (const auto& name : deleted_topics_) {
            out << "DELTOPIC\t" << name << '\n';
        }
    }

    void showHistory() const {
        if (history_.empty()) {
            printErr("还没有执行过命令");
            return;
        }
        std::cout << color::Cyan << color::Bold << "最近命令" << color::Reset << "\n";
        std::size_t index = 1;
        for (const auto& item : history_) {
            std::cout << "  " << index++ << ". " << item << "\n";
        }
    }

    void printHelp() const {
        std::cout << color::Yellow << color::Bold << "可用命令：" << color::Reset << "\n";
        std::cout << color::Dim
                  << "  pub     <名字>                    创建发布者（尚未绑定话题）\n"
                  << "  sub     <名字>                    创建订阅者（尚未订阅话题）\n"
                  << "  topic   <发布者> <话题>           创建新话题并绑定\n"
                  << "  join    <发布者> <话题>           绑定到已存在的话题\n"
                  << "  listen  <订阅者> <话题>           订阅已存在的话题\n"
                  << "  send    <发布者> <话题> <内容>    向指定话题发消息\n"
                  << "  recv    <订阅者>                  查看该订阅者收到的消息\n"
                  << "  del     pub|sub|topic <名字>      删除发布者 / 订阅者 / 话题\n"
                  << "  info    <话题>                    查看话题详情（含已注销发布者）\n"
                  << "  history                            查看最近执行的命令\n"
                  << "  clear                             一键清空所有话题、发布者和订阅者\n"
                  << "  clearlog                          清空日志文件\n"
                  << "  list                              查看话题与节点\n"
                  << "  help                              显示帮助\n"
                  << "  exit                              保存并退出" << color::Reset << "\n";
    }

    static void narrative(const std::string& text) {
        ros_sim::Logger::instance().info(text);
    }

    std::map<std::string, PublisherMap> active_publishers_; // 节点名 -> 话题 -> 发布者
    std::unordered_map<std::string, std::shared_ptr<ros_sim::Subscriber>> subscribers_;
    std::set<std::string> pending_publishers_;
    std::set<std::string> pending_subscribers_;
    std::set<std::string> deleted_topics_;
    std::vector<std::string> history_;
    std::map<std::string, TopicInfo> topics_;
};

} // namespace

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8); // 让 Windows 控制台按 UTF-8 显示中文
    // 开启 ANSI 转义支持，彩色输出才能生效
    HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (output != INVALID_HANDLE_VALUE && GetConsoleMode(output, &mode)) {
        SetConsoleMode(output, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif

    Console console;
    console.run();
    return 0;
}
