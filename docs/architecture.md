# 架构设计文档

| 项目 | 内容 |
| --- | --- |
| 项目名称 | ros_sim —— C++ 模拟 ROS 节点通信系统 |
| 文档版本 | v1.0 |
| 编写日期 | 2026-11-16 |
| 作者 | 潘书逸 |
| 关联文档 | `docs/requirements.md` |

---

## 1. 设计目标与原则

### 1.1 目标

1. 用尽量少的类完整表达「节点—话题—消息」的发布/订阅模型；
2. 每个类职责单一，结构清晰，便于讲解与答辩；
3. 异常路径与正常路径同等对待，非法输入不导致崩溃；
4. 为加分项预留扩展点，新增功能不推翻现有结构。

### 1.2 原则

| 原则 | 在本项目中的落实 |
| --- | --- |
| 单一职责 | 节点只负责自身行为，转发只由 ROS 负责 |
| 依赖倒置 | ROS 持有节点的抽象基类指针，不依赖具体实现 |
| RAII | 全部使用智能指针，无手动 new/delete |
| 开闭原则 | 新增消息类型通过继承 Message，转发逻辑不变 |
| 最小惊讶 | 接口命名直白，异常信息带节点名或话题名 |

---

## 2. 总体架构

### 2.1 分层

```
┌─────────────────────────────────────────────────────┐
│  应用层  main.cpp（run_demo + 全局异常捕获）           │
└──────────────────────────┬──────────────────────────┘
                           │ 创建、init、publish
┌──────────────────────────▼──────────────────────────┐
│  核心层  ROS（单例）                                  │
│  · topic → vector<shared_ptr<Publisher>>             │
│  · topic → vector<shared_ptr<Subscriber>>            │
│  · nodeNames（查重）                                  │
│  · publish / shutdownAll                             │
└───────────┬──────────────────────────────┬──────────┘
            │ 持有（shared_ptr）            │ 持有（shared_ptr）
┌───────────▼──────────┐        ┌──────────▼───────────┐
│  Publisher           │        │  Subscriber          │
│  继承 Node           │        │  继承 Node           │
└───────────┬──────────┘        └──────────┬───────────┘
            └──────────────┬───────────────┘
┌──────────────────────────▼──────────────────────────┐
│  模型层  Node（抽象基类） / Message / SensorMessage   │
└──────────────────────────┬──────────────────────────┘
┌──────────────────────────▼──────────────────────────┐
│  基础设施  Logger（单例）/ 异常体系                    │
└─────────────────────────────────────────────────────┘
```

### 2.2 模块职责

| 模块 | 负责 | 不负责 |
| --- | --- | --- |
| Node | 名称校验、生命周期接口定义 | 具体通信行为 |
| Publisher | 组装消息、触发转发、记录日志 | 查找订阅者 |
| Subscriber | 接收消息、记录日志 | 主动拉取 |
| Message | 内容与时间戳的不可变封装 | 序列化、网络传输 |
| SensorMessage | 在 Message 基础上增加温湿度 | 转发逻辑 |
| ROS | 注册表维护、查重、转发、统一关闭 | 解释消息内容 |
| Logger | 分级日志、控制台与文件双输出 | 业务逻辑 |
| 异常体系 | 分类表达错误、携带上下文 | 错误恢复 |
| ros_console | 命令解析、运行时增删节点、状态持久化 | 通信逻辑本身 |

---

## 3. 类设计

### 3.1 类关系

```
Node（抽象） ──┬── Publisher
               └── Subscriber

Message ──── SensorMessage（派生）

ROS（单例）── 持有 Publisher / Subscriber 的 shared_ptr

RosException
 ├── NodeException
 ├── TopicException
 └── MessageException

Logger（单例）
```

### 3.2 Node

| 成员 | 类型 | 说明 |
| --- | --- | --- |
| `name_` | `std::string` | 全局唯一名称，构造时校验 |
| `name()` | `const std::string&` | 只读访问 |
| `init()` | 纯虚 | 子类实现初始化与注册 |
| `shutdown()` | 纯虚 | 子类实现关闭 |
| `~Node()` | 虚析构 | 多态删除安全 |

**设计要点**

- 继承 `std::enable_shared_from_this<Node>`，使 `init()` 内部能取得自身的 `shared_ptr` 用于注册；
- 名称合法性在构造函数中检查，非法名称不会产生“半合法对象”；
- 纯虚接口强制子类实现，漏写会在编译期报错。

### 3.3 Publisher

| 成员 | 说明 |
| --- | --- |
| `topic_` | 话题名，构造时校验非空 |
| `initialized_` | 是否已初始化，防止重复初始化 |
| `init()` | 输出日志 → 调用 `ROS::registerPublisher` |
| `shutdown()` | 输出日志，未初始化时记录 WARN |
| `publish(const std::string&)` | 构造 Message 后转发 |
| `publish(const Message&)` | 直接转发已有消息（支持自定义消息） |

### 3.4 Subscriber

| 成员 | 说明 |
| --- | --- |
| `topic_` | 话题名 |
| `initialized_` | 是否已初始化 |
| `init()` | 输出日志 → 调用 `ROS::registerSubscriber` |
| `shutdown()` | 输出日志 |
| `receive(const Message&)` | 通过虚函数 `toString()` 打印，兼容自定义消息 |

### 3.5 Message

| 成员 | 说明 |
| --- | --- |
| `content_` | 业务内容，构造时校验非空 |
| `timestamp_` | 构造时自动生成，格式 `YYYY-MM-DD HH:MM:SS` |
| `content()` / `timestamp()` | 只读访问，返回 `const std::string&` |
| `toString()` | 虚函数，返回「内容 @ 时间戳」 |

**不可变性**：成员全部私有，只提供 const 访问器，没有 setter。

### 3.6 SensorMessage

继承 Message，额外持有 `temperature_` 与 `humidity_`，重写 `toString()` 输出温度湿度。通过基类引用传递时依赖虚函数实现多态，不会发生对象切片。

### 3.7 ROS（单例）

| 成员 | 说明 |
| --- | --- |
| `publishers_` | `unordered_map<string, vector<shared_ptr<Publisher>>>` |
| `subscribers_` | `unordered_map<string, vector<shared_ptr<Subscriber>>>` |
| `nodeNames_` | `unordered_set<string>`，用于名称查重 |
| `registerPublisher` / `registerSubscriber` | 校验空指针、重复名称后写入映射表 |
| `publish` | 查找订阅者并逐个投递，无订阅者抛 TopicException |
| `shutdownAll` | 遍历全部节点调用 shutdown，然后清空三张表 |

### 3.8 异常体系

```
std::runtime_error
 └── RosException
      ├── NodeException      节点：空名、重名、重复初始化
      ├── TopicException     话题：空话题、话题不存在
      └── MessageException   消息：空内容
```

继承 `std::runtime_error` 的原因：兼容标准库异常体系，`what()` 可直接用于日志，外层用 `catch (const std::exception&)` 兜底。

### 3.9 Logger

单例，提供 Info / Warn / Error 三级接口。每条日志格式：

```
[时间] [级别] 内容
```

同时输出到控制台和 `logs/ros_sim.log`（追加模式）。文件打开失败时自动降级为仅控制台输出，不抛异常。

---

## 4. 关键设计决策（ADR）

### ADR-01 单例采用 Meyers 写法

```cpp
ROS& ROS::instance() {
    static ROS instance;
    return instance;
}
```

- **理由**：C++11 起静态局部变量初始化线程安全；无需手动释放；避免静态初始化顺序问题。
- **代价**：销毁时机由运行时决定，对本项目无影响。
- **备选**：手动 `new` + 静态指针，需要额外管理释放且易泄漏，否决。

### ADR-02 注册表用 `unordered_map<string, vector<shared_ptr<T>>>`

- **理由**：按话题查找平均 O(1)；`vector` 保持注册顺序，转发顺序可预期。
- **代价**：需要额外维护 `nodeNames_` 做查重。

### ADR-03 节点由 `shared_ptr` 管理，ROS 持有强引用

- **理由**：节点注册后生命周期长于局部作用域，强引用避免悬垂指针；`enable_shared_from_this` 让 `init()` 内可取得自身智能指针。
- **约束**：节点必须用 `std::make_shared` 创建，否则 `shared_from_this()` 抛 `bad_weak_ptr`。
- **备选**：裸指针 + 析构自动注销，因基类析构无法调用纯虚函数且易漏注销，否决。

### ADR-04 消息以 `const Message&` 传递

- **理由**：避免拷贝；避免按值传递导致对象切片（SensorMessage 的温湿度会丢失）；同步调用保证引用始终有效。
- **备选**：`shared_ptr<const Message>`，适合多线程或异步队列，本期不需要。

### ADR-05 发布采用同步分发，不引入线程

- **理由**：需求中的“异步”指发布方不被订阅方阻塞。单线程下投递完立即返回即满足；线程会带来竞态与调试成本。
- **扩展**：将来可改为消息队列 + 工作线程，接口不变。

### ADR-06 时间戳使用 `system_clock` + 本地时间转换

- `std::time(nullptr)` 取当前时间，`std::localtime` 转换，`std::put_time` 格式化。
- **已知限制**：`localtime` 非线程安全，本期单线程可接受；多线程版本应改用 `localtime_s` / `localtime_r`。

### ADR-07 异常策略分三层

| 层次 | 策略 |
| --- | --- |
| 构造 / 注册 | 校验失败立即抛出，不留下半初始化状态 |
| 转发 | 单个订阅者异常被捕获并记 ERROR，继续投递其余订阅者 |
| 顶层 | main 分类捕获，输出错误日志，返回退出码 1 |

### ADR-08 日志器绝不抛异常

日志是辅助手段，若在错误处理路径中再抛会导致 `std::terminate`。Logger 内部吞掉所有 I/O 异常，文件打不开时降级为仅控制台。

### ADR-09 头文件依赖方向

`publisher.hpp`、`subscriber.hpp` 不包含 `ros.hpp`，仅在 `.cpp` 中包含，避免循环依赖；`ros.hpp` 包含前两者。全部头文件使用 `#pragma once`。

### ADR-10 构建拆分为核心库 + 两个可执行程序

- `ros_core`（静态库）：除 main 外的全部实现；
- `ros_sim`：演示程序；
- `ros_tests`：测试程序。
- **理由**：主程序与测试各有一个 `main()`，必须分开链接；拆库后两边共享同一份实现。

### ADR-11 Windows 控制台按 UTF-8 输出

```cpp
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
```

- **理由**：源码与日志均为 UTF-8，Windows 控制台默认 GBK，会导致中文乱码。
- **代价**：仅 Windows 生效，Linux 下无需处理。

---

## 5. 运行时序

### 5.1 初始化与注册

```
main        Publisher        ROS              Logger
 │             │              │                 │
 │─构造(name,topic)──────────►│                 │
 │─init()─────►│              │                 │
 │             │─registerPublisher(shared_ptr)─►│
 │             │              │─校验空指针/重名──│
 │             │              │─写入 nodeNames_──│
 │             │              │─写入 publishers_─│
 │             │─log(INFO)────────────────────►│
 │◄────────────│              │                 │
```

### 5.2 发布与转发

```
Publisher        ROS          Subscriber A    Subscriber B
 │─publish(data)─►│               │               │
 │ 构造 Message   │               │               │
 │─log(INFO)─────►│               │               │
 │─ROS::publish(topic,msg)──────►│               │
 │                │─查找订阅者列表─│               │
 │                │─receive(msg)─►│               │
 │                │               │─log(INFO)     │
 │                │─receive(msg)─────────────────►│
 │                │               │               │─log(INFO)
 │◄─返回──────────│               │               │
```

### 5.3 统一关闭

```
main        ROS          Publisher      Subscriber
 │─shutdownAll()─►│            │              │
 │                │─shutdown()─►│              │
 │                │             │─log(INFO)    │
 │                │─shutdown()────────────────►│
 │                │                            │─log(INFO)
 │                │─清空映射与名称集合──────────│
 │◄─返回──────────│            │              │
```

### 5.4 异常处理

```
main        run_demo        TopicException
 │─try──────────►│                 │
 │               │─publish 到空话题─►│ 抛出
 │◄──────────────┴─────────────────│
 │ catch (const TopicException&)    │
 │─log(ERROR)───────────────────────│
 │ return 1                         │
```

---

## 6. 接口定义

### 6.1 `exception.hpp`

```cpp
#pragma once
#include <stdexcept>
#include <string>

namespace ros_sim {

class RosException : public std::runtime_error {
public:
    explicit RosException(const std::string& message)
        : std::runtime_error(message) {}
};

class NodeException : public RosException {
public:
    NodeException(const std::string& nodeName, const std::string& message)
        : RosException("[Node:" + nodeName + "] " + message) {}
};

class TopicException : public RosException {
public:
    explicit TopicException(const std::string& message)
        : RosException("[Topic] " + message) {}
};

class MessageException : public RosException {
public:
    explicit MessageException(const std::string& message)
        : RosException("[Message] " + message) {}
};

}  // namespace ros_sim
```

### 6.2 `logger.hpp`

```cpp
#pragma once
#include <fstream>
#include <string>

namespace ros_sim {

enum class LogLevel { Info, Warn, Error };

class Logger {
public:
    static Logger& instance();

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    void log(LogLevel level, const std::string& message);
    void info(const std::string& message);
    void warn(const std::string& message);
    void error(const std::string& message);

private:
    Logger() = default;
    ~Logger() = default;

    static const char* levelName(LogLevel level);

    std::ofstream file_;
};

}  // namespace ros_sim
```

### 6.3 `message.hpp`

```cpp
#pragma once
#include <string>

namespace ros_sim {

class Message {
public:
    explicit Message(std::string content);
    virtual ~Message() = default;

    const std::string& content() const noexcept { return content_; }
    const std::string& timestamp() const noexcept { return timestamp_; }

    virtual std::string toString() const;

private:
    std::string content_;
    std::string timestamp_;
};

}  // namespace ros_sim
```

### 6.4 `sensor_message.hpp`

```cpp
#pragma once
#include <string>

#include "ros/message.hpp"

namespace ros_sim {

class SensorMessage : public Message {
public:
    SensorMessage(std::string content, double temperature, double humidity);

    double temperature() const noexcept { return temperature_; }
    double humidity() const noexcept { return humidity_; }

    std::string toString() const override;

private:
    double temperature_;
    double humidity_;
};

}  // namespace ros_sim
```

### 6.5 `node.hpp`

```cpp
#pragma once
#include <memory>
#include <string>

namespace ros_sim {

class Node : public std::enable_shared_from_this<Node> {
public:
    explicit Node(std::string name);
    virtual ~Node() = default;

    const std::string& name() const noexcept { return name_; }

    virtual void init() = 0;
    virtual void shutdown() = 0;

protected:
    static std::string validateName(std::string name);

private:
    std::string name_;
};

}  // namespace ros_sim
```

### 6.6 `publisher.hpp`

```cpp
#pragma once
#include <string>

#include "ros/message.hpp"
#include "ros/node.hpp"

namespace ros_sim {

class Publisher final : public Node {
public:
    Publisher(std::string name, std::string topic);

    void init() override;
    void shutdown() override;

    const std::string& topic() const noexcept { return topic_; }

    void publish(const std::string& sensorData);
    void publish(const Message& message);

private:
    std::string topic_;
    bool initialized_{false};
};

}  // namespace ros_sim
```

### 6.7 `subscriber.hpp`

```cpp
#pragma once
#include <string>

#include "ros/message.hpp"
#include "ros/node.hpp"

namespace ros_sim {

class Subscriber final : public Node {
public:
    Subscriber(std::string name, std::string topic);

    void init() override;
    void shutdown() override;

    const std::string& topic() const noexcept { return topic_; }

    void receive(const Message& message);

private:
    std::string topic_;
    bool initialized_{false};
};

}  // namespace ros_sim
```

### 6.8 `ros.hpp`

```cpp
#pragma once
#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ros/message.hpp"
#include "ros/publisher.hpp"
#include "ros/subscriber.hpp"

namespace ros_sim {

class ROS final {
public:
    static ROS& instance();

    ROS(const ROS&) = delete;
    ROS& operator=(const ROS&) = delete;
    ROS(ROS&&) = delete;
    ROS& operator=(ROS&&) = delete;

    void registerPublisher(const std::shared_ptr<Publisher>& publisher);
    void registerSubscriber(const std::shared_ptr<Subscriber>& subscriber);

    void publish(const std::string& topic, const Message& message);

    bool hasSubscriber(const std::string& topic) const;
    std::size_t publisherCount(const std::string& topic) const;
    std::size_t subscriberCount(const std::string& topic) const;

    void shutdownAll();

private:
    ROS() = default;
    ~ROS() = default;

    void ensureNameAvailable(const std::string& name) const;

    std::unordered_map<std::string, std::vector<std::shared_ptr<Publisher>>> publishers_;
    std::unordered_map<std::string, std::vector<std::shared_ptr<Subscriber>>> subscribers_;
    std::unordered_set<std::string> nodeNames_;
};

}  // namespace ros_sim
```

---

### 6.9 `util.hpp`

```cpp
#pragma once
#include <string>

namespace ros_sim {

// 生成 "YYYY-MM-DD HH:MM:SS" 格式的当前时间
std::string nowTimestamp();

// 校验话题名：空或全空白抛 TopicException
std::string validateTopicName(std::string topic);

}  // namespace ros_sim
```

> Logger 与 Message 共用 `nowTimestamp()`，Publisher 与 Subscriber 共用 `validateTopicName()`，避免重复实现。

---

## 7. 目录结构与构建

```
ros-sim/
├── CMakeLists.txt
├── include/ros/
│   ├── exception.hpp
│   ├── logger.hpp
│   ├── message.hpp
│   ├── sensor_message.hpp
│   ├── node.hpp
│   ├── publisher.hpp
│   ├── subscriber.hpp
│   ├── util.hpp
│   └── ros.hpp
├── src/
│   ├── logger.cpp
│   ├── message.cpp
│   ├── sensor_message.cpp
│   ├── node.cpp
│   ├── publisher.cpp
│   ├── subscriber.cpp
│   ├── util.cpp
│   ├── ros.cpp
│   ├── console_main.cpp
│   └── main.cpp
├── tests/
│   └── test_main.cpp
├── docs/
└── README.md
```

构建命令：

```bash
cmake -S . -B build
cmake --build build
```

产物：`ros_sim`（演示）、`ros_tests`（测试）、`ros_console`（交互式控制台）。

---

## 8. 扩展设计（加分项）

| 扩展 | 实现方式 | 对现有代码的影响 |
| --- | --- | --- |
| 自定义消息 | 继承 Message，重写 toString() | 无，转发逻辑不变 |
| 日志持久化 | Logger 内部写文件，失败降级 | 无 |
| 全局异常处理 | main 分类捕获，返回退出码 | 无 |
| 服务/客户端（未实现） | 新增 Service/Client 类 + ROS 服务注册表 | 新增接口，不改现有 |
| 动态增删节点（未实现） | ROS 增加 unregister 接口 | 新增接口 |

---

## 9. 测试策略

| 层次 | 方式 | 覆盖 |
| --- | --- | --- |
| 单元测试 | 自写断言（`tests/test_main.cpp`） | 消息校验、名称校验、查重、转发、关闭清空 |
| 集成测试 | 运行 `ros_sim` | 多发布者/多订阅者、完整生命周期、异常场景 |

测试用例共 14 条，覆盖正常、边界、异常三类。执行方式：运行 `ros_tests`，退出码 0 表示全部通过。

---

## 10. 风险与对策

| 风险 | 影响 | 对策 |
| --- | --- | --- |
| 节点悬垂指针 | 关闭时崩溃 | 由 ROS 持有 shared_ptr，最后统一 shutdownAll |
| 对象切片 | 自定义消息字段丢失 | 一律以 `const Message&` 传递 |
| 时间戳线程安全 | 多线程下数据竞争 | 本期单线程；后续换 localtime_s/r |
| 日志中抛异常 | std::terminate | Logger 内部吞掉所有 I/O 异常 |
| 中文路径导致调试器失效 | 无法调试 | 使用英文路径访问项目 |
| 控制台编码不一致 | 中文乱码 | main 中设置 CP_UTF8 |

---

## 11. 提交计划与实际记录

| 序号 | 提交信息 | 状态 |
| --- | --- | --- |
| 1 | chore: 初始化项目骨架 | ✅ |
| 2 | docs: 建立需求文档提纲 | ✅ |
| 3 | docs: 补全需求文档初稿 | ✅ |
| 4 | feat: 添加异常类 | ✅ |
| 5 | feat: 实现日志器与日志落盘 | ✅ |
| 6 | feat: 实现消息封装与时间戳 | ✅ |
| 7 | feat: 实现 Node 抽象基类 | ✅ |
| 8 | feat: 实现 ROS 单例与注册表 | ✅ |
| 9 | feat: 实现发布者订阅者与多节点演示 | ✅ |
| 10 | feat: 添加全局异常捕获与自定义传感器消息 | ✅ |
| 11 | test: 补充结构化测试用例 | ✅ |
| 12 | docs: 补全架构设计文档 | ✅ |
| 13 | feat: 交互式控制台、运行时动态增删、状态持久化、界面配色 | ✅ |

> 后续的界面调整与文档更新也按功能拆分提交，完整记录见 `git log --oneline`。
