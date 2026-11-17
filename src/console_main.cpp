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

// 交互式控制台
// 发布者：先创建，再让它创建话题或加入已有话题
// 订阅者：先创建，再绑定到已存在的话题
// 控制台只显示交互内容，系统日志写入 logs/ros_sim.log
class Console {
public:
    void run() {
        ros_sim::Logger::instance().setConsoleEnabled(false);
        loadState();
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
            const bool keepGoing = handle(line);
            saveState();   // 每条命令后都保存，直接关窗口也不会丢
            if (!keepGoing) {
                break;
            }
        }

        ros_sim::ROS::instance().shutdownAll();
        std::cout << "已退出（状态已保存，下次启动会恢复）\n";
    }

private:
    struct TopicInfo {
        std::string creator;
        std::vector<std::string> publishers;
        std::set<std::string> removed_publishers;
        std::size_t message_count{0};
    };

    bool handle(const std::string& line) {
        std::istringstream iss(line);
        std::string command;
        iss >> command;

        if (command == "help") { printHelp(); return true; }
        if (command == "exit" || command == "quit") { return false; }
        if (command == "list") { list(); return true; }
        if (command == "pub") { createPublisher(iss); return true; }
        if (command == "sub") { createSubscriber(iss); return true; }
        if (command == "topic") { assignTopic(iss); return true; }
        if (command == "join") { joinTopic(iss); return true; }
        if (command == "listen") { bindTopic(iss); return true; }
        if (command == "send") { send(iss); return true; }
        if (command == "recv") { recv(iss); return true; }
        if (command == "del") { removeNode(iss); return true; }
        if (command == "info") { showTopic(iss); return true; }

        std::cout << "未知命令: " << command << "，输入 help 查看帮助\n";
        return true;
    }

    bool nameTaken(const std::string& name) const {
        return pending_publishers_.count(name) != 0 || active_publishers_.count(name) != 0 ||
               pending_subscribers_.count(name) != 0 || subscribers_.count(name) != 0;
    }

    // pub <名字>：只创建发布者
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
        std::cout << "发布者 " << name << " 已创建（尚未分配话题）\n";
        narrative("[Node:" + name + "] 发布者已创建（尚未分配话题）");
    }

    // sub <名字>：只创建订阅者
    void createSubscriber(std::istringstream& iss) {
        std::string name;
        if (!(iss >> name)) {
            std::cout << "用法: sub <名字>\n";
            return;
        }
        if (nameTaken(name)) {
            std::cout << "名字 " << name << " 已被占用\n";
            return;
        }

        pending_subscribers_.insert(name);
        std::cout << "订阅者 " << name << " 已创建（尚未订阅话题）\n";
        narrative("[Node:" + name + "] 订阅者已创建（尚未订阅话题）");
    }

    // topic <发布者> <话题>：发布者创建话题
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

        const bool was_deleted = deleted_topics_.count(topic) != 0;
        if (was_deleted) {
            deleted_topics_.erase(topic);
        }

        try {
            auto publisher = std::make_shared<ros_sim::Publisher>(name, topic);
            publisher->init();
            active_publishers_[name] = publisher;
            pending_publishers_.erase(name);
            topics_[topic] = TopicInfo{name, {name}, {}, 0};
            std::cout << "话题 " << topic << " 已创建，创建者 " << name << "\n";
            narrative("[Topic:" + topic + "] 话题由 " + name + " 创建");
        } catch (const std::exception& e) {
            std::cout << "创建话题失败: " << e.what() << "\n";
        }
    }

    // join <发布者> <话题>：发布者加入已有话题
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
        if (deleted_topics_.count(topic) != 0) {
            std::cout << "话题 " << topic << " 已被删除，请先重新创建\n";
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
            std::cout << "发布者 " << name << " 已加入话题 " << topic << "\n";
            narrative("[Topic:" + topic + "] 发布者 " + name + " 加入了话题");
        } catch (const std::exception& e) {
            std::cout << "加入话题失败: " << e.what() << "\n";
        }
    }

    // listen <订阅者> <话题>：订阅者绑定到已存在的话题
    void bindTopic(std::istringstream& iss) {
        std::string name;
        std::string topic;
        if (!(iss >> name >> topic)) {
            std::cout << "用法: listen <订阅者> <话题>\n";
            return;
        }
        if (pending_subscribers_.count(name) == 0) {
            if (subscribers_.count(name) != 0) {
                std::cout << "订阅者 " << name << " 已经订阅了话题\n";
            } else {
                std::cout << "没有这个订阅者: " << name << "\n";
            }
            return;
        }
        if (topics_.count(topic) == 0) {
            if (deleted_topics_.count(topic) != 0) {
                std::cout << "话题 " << topic << " 已被删除，请先重新创建\n";
            } else {
                std::cout << "话题 " << topic << " 还没有创建，不能订阅\n";
            }
            return;
        }

        try {
            auto subscriber = std::make_shared<ros_sim::Subscriber>(name, topic);
            subscriber->init();
            subscribers_[name] = subscriber;
            pending_subscribers_.erase(name);
            std::cout << "订阅者 " << name << " 已订阅话题 " << topic << "\n";
            narrative("[Topic:" + topic + "] 订阅者 " + name + " 订阅了话题");
        } catch (const std::exception& e) {
            std::cout << "订阅失败: " << e.what() << "\n";
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
            auto topicIt = topics_.find(topic);
            if (topicIt != topics_.end()) {
                ++topicIt->second.message_count;
            }
            std::cout << "已发送到 " << topic << "，"
                      << ros_sim::ROS::instance().subscriberCount(topic) << " 个订阅者收到\n";
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

        if (pending_subscribers_.count(name) != 0) {
            std::cout << "订阅者 " << name << " 还没有订阅话题\n";
            return;
        }

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

    // del pub|sub|topic <名字>
    void removeNode(std::istringstream& iss) {
        std::string type;
        std::string name;
        if (!(iss >> type >> name)) {
            std::cout << "用法: del pub|sub|topic <名字>\n";
            return;
        }

        if (type == "pub") {
            removePublisher(name);
        } else if (type == "sub") {
            removeSubscriber(name);
        } else if (type == "topic") {
            removeTopic(name);
        } else {
            std::cout << "用法: del pub|sub|topic <名字>\n";
        }
    }

    void removePublisher(const std::string& name) {
        if (pending_publishers_.count(name) != 0) {
            pending_publishers_.erase(name);
            std::cout << "已删除发布者 " << name << "（它还没有话题）\n";
            narrative("[Node:" + name + "] 发布者已删除");
            return;
        }

        auto it = active_publishers_.find(name);
        if (it == active_publishers_.end()) {
            std::cout << "没有这个发布者: " << name << "\n";
            return;
        }

        const std::string topic = it->second->topic();
        try {
            it->second->shutdown();
            ros_sim::ROS::instance().unregisterPublisher(it->second);
            active_publishers_.erase(it);

            auto topicIt = topics_.find(topic);
            if (topicIt != topics_.end()) {
                auto& list = topicIt->second.publishers;
                list.erase(std::remove(list.begin(), list.end(), name), list.end());
                topicIt->second.removed_publishers.insert(name);
            }
            std::cout << "已删除发布者 " << name << "（话题 " << topic
                      << " 保留，它发过的消息仍在订阅者手里）\n";
            narrative("[Node:" + name + "] 发布者已删除，话题 " + topic + " 保留");
        } catch (const std::exception& e) {
            std::cout << "删除失败: " << e.what() << "\n";
        }
    }

    void removeSubscriber(const std::string& name) {
        if (pending_subscribers_.count(name) != 0) {
            pending_subscribers_.erase(name);
            std::cout << "已删除订阅者 " << name << "（它还没有订阅话题）\n";
            narrative("[Node:" + name + "] 订阅者已删除");
            return;
        }

        auto it = subscribers_.find(name);
        if (it == subscribers_.end()) {
            std::cout << "没有这个订阅者: " << name << "\n";
            return;
        }

        const std::string topic = it->second->topic();
        try {
            it->second->shutdown();
            ros_sim::ROS::instance().unregisterSubscriber(it->second);
            subscribers_.erase(it);
            std::cout << "已删除订阅者 " << name << "（已从话题 " << topic << " 退订）\n";
            narrative("[Node:" + name + "] 订阅者已删除，已从话题 " + topic + " 退订");
        } catch (const std::exception& e) {
            std::cout << "删除失败: " << e.what() << "\n";
        }
    }

    void removeTopic(const std::string& topic) {
        auto it = topics_.find(topic);
        if (it == topics_.end()) {
            std::cout << "没有这个话题: " << topic << "\n";
            return;
        }
        if (!it->second.publishers.empty() ||
            ros_sim::ROS::instance().subscriberCount(topic) != 0) {
            std::cout << "话题 " << topic << " 还有发布者或订阅者，请先删除它们\n";
            return;
        }

        topics_.erase(it);
        deleted_topics_.insert(topic);
        std::cout << "已删除话题 " << topic << "\n";
        narrative("[Topic:" + topic + "] 话题已删除");
    }

    void showTopic(std::istringstream& iss) {
        std::string topic;
        if (!(iss >> topic)) {
            std::cout << "用法: info <话题>\n";
            return;
        }

        auto it = topics_.find(topic);
        if (it == topics_.end()) {
            if (deleted_topics_.count(topic) != 0) {
                std::cout << "话题 " << topic << " 已被删除\n";
            } else {
                std::cout << "没有这个话题: " << topic << "\n";
            }
            return;
        }

        const TopicInfo& info = it->second;
        std::cout << "话题: " << topic << "\n";
        std::cout << "  创建者: " << info.creator << "\n";
        std::cout << "  发布者:\n";
        for (const auto& name : info.publishers) {
            std::cout << "    " << name << "（活动中）\n";
        }
        for (const auto& name : info.removed_publishers) {
            std::cout << "    " << name << "（已注销）\n";
        }
        if (info.publishers.empty() && info.removed_publishers.empty()) {
            std::cout << "    （无）\n";
        }
        std::cout << "  订阅者: " << ros_sim::ROS::instance().subscriberCount(topic) << "\n";
        std::cout << "  消息记录: " << info.message_count << " 条\n";
    }

    void list() const {
        std::cout << "话题: " << topics_.size() << " 个\n";
        for (const auto& entry : topics_) {
            std::cout << "  " << entry.first << "  创建者: " << entry.second.creator
                      << "  发布者: " << entry.second.publishers.size()
                      << "  订阅者: " << ros_sim::ROS::instance().subscriberCount(entry.first)
                      << "  消息: " << entry.second.message_count << " 条";
            if (!entry.second.removed_publishers.empty()) {
                std::cout << "  已注销发布者:";
                for (const auto& name : entry.second.removed_publishers) {
                    std::cout << " " << name;
                }
            }
            std::cout << "\n";
        }

        std::cout << "发布者: " << (pending_publishers_.size() + active_publishers_.size())
                  << " 个\n";
        for (const auto& name : pending_publishers_) {
            std::cout << "  " << name << " -> (还没有话题)\n";
        }
        for (const auto& entry : active_publishers_) {
            std::cout << "  " << entry.first << " -> " << entry.second->topic() << "\n";
        }

        std::cout << "订阅者: " << (pending_subscribers_.size() + subscribers_.size())
                  << " 个\n";
        for (const auto& name : pending_subscribers_) {
            std::cout << "  " << name << " -> (还没有订阅话题)\n";
        }
        for (const auto& entry : subscribers_) {
            std::cout << "  " << entry.first << " -> " << entry.second->topic()
                      << "（已收 " << entry.second->inbox().size() << " 条）\n";
        }
    }

    static const char* stateFile() { return "console_state.txt"; }

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
                topicRecords.emplace_back(fields[1], fields[2], static_cast<std::size_t>(std::stoul(fields[3])));
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
                auto publisher = std::make_shared<ros_sim::Publisher>(record.first, record.second);
                publisher->init();
                active_publishers_[record.first] = publisher;
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
                auto subscriber = std::make_shared<ros_sim::Subscriber>(record.first, record.second);
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
                  << active_publishers_.size() << " 个，订阅者 " << subscribers_.size()
                  << " 个\n";
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
        for (const auto& entry : active_publishers_) {
            out << "PUB\t" << entry.first << '\t' << entry.second->topic() << '\n';
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

    void printHelp() const {
        std::cout << "可用命令:\n"
                  << "  pub     <名字>            创建发布者（此时还没有话题）\n"
                  << "  sub     <名字>            创建订阅者（此时还没有订阅话题）\n"
                  << "  topic   <发布者> <话题>   让该发布者创建话题\n"
                  << "  join    <发布者> <话题>   让该发布者加入已存在的话题\n"
                  << "  listen  <订阅者> <话题>   让该订阅者订阅已存在的话题\n"
                  << "  send    <发布者> <内容>   用该发布者发消息\n"
                  << "  recv    <订阅者>          查看该订阅者收到的消息\n"
                  << "  del     pub|sub|topic <名字>  删除发布者 / 订阅者 / 话题\n"
                  << "  info    <话题>            查看话题详情（含已注销发布者）\n"
                  << "  list                      查看话题与节点\n"
                  << "  help                      显示帮助\n"
                  << "  exit                      关闭所有节点并退出\n";
    }

    static void narrative(const std::string& text) {
        ros_sim::Logger::instance().info(text);
    }

    std::unordered_map<std::string, std::shared_ptr<ros_sim::Publisher>> active_publishers_;
    std::unordered_map<std::string, std::shared_ptr<ros_sim::Subscriber>> subscribers_;
    std::set<std::string> pending_publishers_;
    std::set<std::string> pending_subscribers_;
    std::set<std::string> deleted_topics_;
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
