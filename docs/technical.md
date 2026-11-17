# 技术文档

| 项目 | 内容 |
| --- | --- |
| 项目名称 | ros_sim —— C++ 模拟 ROS 节点通信系统 |
| 文档版本 | v1.0 |
| 编写日期 | 2026-11-17 |
| 作者 | 潘书逸 |

---

## 1. 项目概述

本项目用 C++17 标准库实现了一个简易的 ROS 通信内核，包含节点抽象、发布者、订阅者、消息封装、单例通信核心、异常体系与日志系统。程序不依赖任何第三方库，一条 `cmake` 命令即可构建。

最终交付两个可执行程序：

| 程序 | 作用 |
| --- | --- |
| `ros_sim` | 演示程序：多话题、多订阅者的完整生命周期 |
| `ros_tests` | 测试程序：14 个用例，覆盖正常、边界、异常三类场景 |

---

## 2. 开发环境与技术选型

### 2.1 开发环境

| 项 | 版本 |
| --- | --- |
| 操作系统 | Windows 11 |
| IDE | CLion 2026.2 |
| 编译器 | MinGW GCC 15.2（CLion 自带） |
| 构建工具 | CMake 4.x + Ninja |
| 语言标准 | C++17 |
| 版本管理 | Git 2.56 |

### 2.2 为什么选这些

**为什么用 C++17**

智能指针、`std::filesystem`、结构化绑定、`enum class` 都是 C++17 里用起来最顺手的。比如创建日志目录用的是 `std::filesystem::create_directories`，换成 C++11 就得调平台 API。

**为什么用 CMake**

课程要求跨平台，CMake 在 Windows 和 Linux 上都能用，而且 CLion 原生支持。相比手写 Makefile，CMake 更省事。

**为什么不用第三方库**

课程要求“只用标准库”。实际做下来发现也确实够用：容器用 STL，异常用 `std::runtime_error`，日志用 `ofstream`，没有必须引第三方的地方。

**为什么全程用智能指针**

节点注册到 ROS 核心后，生命周期比局部变量长。如果用裸指针，很容易出现“节点已销毁但注册表还留着指针”的悬垂问题。用 `shared_ptr` 由 ROS 持有引用，从根上避免这类崩溃。

---

## 3. 模块实现说明

### 3.1 Node（节点抽象基类）

`Node` 只做三件事：保存名称、校验名称、定义生命周期接口。

```cpp
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
```

名称校验放在构造函数里，用 `find_first_not_of(" \t\r\n")` 判断是否为空或全空白。这样非法名称根本创建不出对象，不会出现“半合法”的节点。

`init()` 和 `shutdown()` 声明为纯虚，子类必须实现，漏写直接编译报错。

### 3.2 Publisher（发布者）

初始化分三步：检查是否重复初始化 → 打印日志 → 向 ROS 注册。

```cpp
void Publisher::init() {
    if (initialized_) {
        throw NodeException(name(), "重复初始化");
    }
    Logger::instance().info("[Node:" + name() + "] 发布者初始化完成 topic=" + topic_);
    ROS::instance().registerPublisher(std::static_pointer_cast<Publisher>(shared_from_this()));
    initialized_ = true;
}
```

`shared_from_this()` 来自基类的 `enable_shared_from_this`，作用是拿到“指向自己的 shared_ptr”，这样才能安全地交给 ROS 保存。这也是为什么节点必须用 `make_shared` 创建。

发布有两个重载：一个接收字符串（自动构造 Message），一个直接接收 Message 对象（支持自定义消息）。

### 3.3 Subscriber（订阅者）

订阅者的 `receive` 只做一件事：打印。

```cpp
void Subscriber::receive(const Message& message) {
    Logger::instance().info("[Node:" + name() + "] 收到消息: " + message.toString());
}
```

这里调用的是 `toString()` 而不是 `content()`，原因见第 4.1 节。

### 3.4 Message 与 SensorMessage

`Message` 保存内容和时间戳，构造时校验内容非空，时间戳自动生成。所有成员私有，只提供 const 访问器：

```cpp
const std::string& content() const noexcept { return content_; }
const std::string& timestamp() const noexcept { return timestamp_; }
```

由于没有任何 setter，消息一旦创建就不可修改，满足“封装性”要求。

`SensorMessage` 继承 Message，多了温度和湿度两个字段，重写 `toString()`：

```cpp
std::string SensorMessage::toString() const {
    std::ostringstream oss;
    oss << Message::toString() << " | 温度=" << std::fixed << std::setprecision(1)
        << temperature_ << " 湿度=" << std::fixed << std::setprecision(1) << humidity_;
    return oss.str();
}
```

### 3.5 ROS（通信核心）

ROS 是单例，内部维护三张表：

| 成员 | 类型 | 用途 |
| --- | --- | --- |
| `publishers_` | `unordered_map<string, vector<shared_ptr<Publisher>>>` | 话题 → 发布者 |
| `subscribers_` | `unordered_map<string, vector<shared_ptr<Subscriber>>>` | 话题 → 订阅者 |
| `nodeNames_` | `unordered_set<string>` | 已注册节点名，用于查重 |

注册时先检查空指针，再检查名称是否重复，最后写入映射表。转发时按话题找到订阅者列表，逐个调用 `receive`：

```cpp
for (const auto& subscriber : it->second) {
    try {
        subscriber->receive(message);
    } catch (const std::exception& e) {
        Logger::instance().error("[Topic:" + topic + "] 转发失败: " + e.what());
    }
}
```

这里对单个订阅者做了 try/catch：某个订阅者处理出错，不影响其他订阅者收到消息。

### 3.6 Logger（日志器）

单例，提供 Info / Warn / Error 三级接口，同时输出到控制台和 `logs/ros_sim.log`。

时间戳格式在 `nowTimestamp()` 里生成：

```cpp
std::time_t now = std::time(nullptr);
std::tm* local = std::localtime(&now);
std::ostringstream oss;
oss << std::put_time(local, "%Y-%m-%d %H:%M:%S");
```

文件采用追加模式打开，程序多次运行不会覆盖旧日志。如果 `logs` 目录不存在，用 `std::filesystem::create_directories` 先创建；如果文件打不开，就只输出到控制台，不抛异常。

### 3.7 异常体系

```
std::runtime_error
 └── RosException
      ├── NodeException      节点相关
      ├── TopicException     话题相关
      └── MessageException   消息相关
```

异常信息统一带上下文前缀，例如：

```
[Node:pub1] 节点名不能为空或全为空白
[Topic] 话题不存在或没有订阅者: /room/none
```

这样看日志就能直接定位是哪个节点、哪个话题出的问题。

---

### 3.10 交互式控制台（ros_console）

`ros_sim` 是写死的演示程序，为了让系统真正“可操作”，额外实现了 `ros_console`：

- 命令解析：`pub` / `sub` / `topic` / `join` / `listen` / `send` / `recv` / `del` / `info` / `clear` / `list` / `history` / `help` / `exit`
- 发布者和订阅者都采用“先创建、再绑定”的两步模型
- 一个发布者可以绑定多个话题，发送时必须指明话题，避免歧义
- `del pub <名字> [话题]`：带话题时只解绑一个话题，不带时删除整个发布者
- `history` 保留最近 20 条命令（控制台读不到方向键，用命令代替）
- 删除话题后保留“已删除”标记，再次使用会提示重新创建
- 注销发布者后，话题保留其“已注销”记录，历史消息不清除
- 界面使用 ANSI 配色：成功绿色 ✔、失败红色 ✘、标题青色，颜色可在代码里调整

### 3.11 状态持久化

控制台把状态保存到 `console_state.txt`，每条命令执行后写一次，启动时恢复。文件是制表符分隔的文本：

```
TOPIC	/t	p1	1
PUB	p1	/t
SUB	s1	/t
MSG	s1	hello @ 2026-11-17 21:23:20
```

加载时先恢复话题与元数据，再重建发布者/订阅者对象并调用 `init()` 注册，最后恢复消息箱。遇到损坏的记录会跳过，不影响启动。

---

## 4. 关键技术点

### 4.1 虚函数与多态

`Message::toString()` 是虚函数，`Subscriber::receive` 接收的是 `const Message&`。传入不同的派生类，打印结果不同：

- 普通 Message → `内容 @ 时间戳`
- SensorMessage → `内容 @ 时间戳 | 温度=25.3 湿度=60.0`

这就是多态：**调用方只认基类接口，实际执行的是子类实现**。

### 4.2 shared_ptr 与 enable_shared_from_this

节点在 `init()` 里需要把自己注册到 ROS，但成员函数里只有 `this`（裸指针）。直接传裸指针给 ROS 保存，一旦节点销毁就会悬垂。

解决方案是让 Node 继承 `enable_shared_from_this<Node>`，调用 `shared_from_this()` 取得指向自己的智能指针：

```cpp
ROS::instance().registerPublisher(std::static_pointer_cast<Publisher>(shared_from_this()));
```

前提是对象必须由 `std::make_shared` 创建，否则会抛 `bad_weak_ptr`。

### 4.3 三层异常策略

| 层次 | 策略 |
| --- | --- |
| 构造 / 注册 | 校验失败立即抛出，不留下半初始化对象 |
| 转发 | 单个订阅者异常被捕获，记 ERROR 后继续投递 |
| 顶层 | main 分类捕获，打印日志，返回退出码 1 |

顶层代码：

```cpp
try {
    run_demo();
} catch (const ros_sim::NodeException& e) { ... return 1; }
  catch (const ros_sim::TopicException& e) { ... return 1; }
  catch (const ros_sim::MessageException& e) { ... return 1; }
  catch (const std::exception& e) { ... return 1; }
```

**顺序不能反**：三个自定义异常都继承自 `std::exception`，如果把 `std::exception` 放在前面，后面的分类处理永远不会执行。

### 4.4 时间戳生成

系统时间用 `std::time(nullptr)` 获取，转本地时间用 `std::localtime`，格式化用 `std::put_time`。

已知限制：`std::localtime` 不是线程安全的。本期程序是单线程，可以接受；如果以后改成多线程，应该换成 `localtime_s`（Windows）或 `localtime_r`（Linux）。

### 4.5 日志落盘与降级

日志文件路径是 `logs/ros_sim.log`，用追加模式：

```cpp
file_.open("logs/ros_sim.log", std::ios::app);
```

目录不存在时先创建。文件打开失败时，`file_.is_open()` 为 false，程序只输出控制台，不影响主流程。

日志器本身绝不抛异常——否则在错误处理路径里再抛异常，会直接触发 `std::terminate`。

### 4.6 Windows 控制台 UTF-8

源码和日志都是 UTF-8，但 Windows 控制台默认用 GBK 解码，中文会变乱码。解决办法是在程序启动时切换控制台代码页：

```cpp
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
```

`ros_sim` 和 `ros_tests` 两个程序都加了一次。

---

## 5. 开发中遇到的问题与解决

这一节记录实际开发中卡住过的地方。

### 5.1 中文路径导致调试器无法加载

**现象**：CLion 点 Debug 时提示

```
Failed to load executable due to non-ASCII characters in the path
```

**原因**：Windows 用户名是中文（`C:\Users\潘书逸\...`），CLion 自带的 GDB 无法加载中文路径下的可执行文件。

**尝试过的方案**：

1. 在 `C:\Users\Public` 下建目录联接，给项目一个英文路径入口；
2. 删除旧的 `cmake-build-debug`，因为缓存里记录的还是中文源码路径；
3. 最后把 CMake 构建目录改到 `C:\Users\Public\ros-build`。

**结论**：让可执行文件的路径完全没有中文，GDB 就能正常工作。编译和运行本身不受中文路径影响，只有调试器有问题。

### 5.2 控制台中文乱码

**现象**：程序在 CLion 的 Run 窗口输出中文变成乱码。

**原因**：程序输出 UTF-8 字节，控制台按 GBK 解释。

**解决**：在 `main()` 开头调用 `SetConsoleOutputCP(CP_UTF8)`。两个可执行程序都要加，漏掉一个，那个程序就还是乱码。

### 5.3 节点名重复导致程序崩溃

**现象**：加了湿度话题后，程序运行到一半直接退出：

```
terminate called after throwing an instance of 'ros_sim::NodeException'
  what():  [Node:sub_display] 节点名重复注册
```

**原因**：复制温度组的代码时，只改了变量名，忘了改引号里的节点名。ROS 注册时查重，发现重复就抛异常，而当时 `main` 还没有 try/catch，异常没人接，程序终止。

**解决**：把节点名改成 `sub_humid_display`、`sub_humid_app`，并补上全局异常捕获。

**教训**：变量名给编译器看，节点名给 ROS 看，两个都要唯一。

### 5.4 新增源文件没有加进 CMake

**现象**：写了 `message.cpp`，编译能过，但改动不生效。

**原因**：`CMakeLists.txt` 的 `add_executable` 里没有这个文件，它根本没参与编译。

**解决**：每次新增 `.cpp` 都要同步修改 `CMakeLists.txt`。后来把构建拆成 `ros_core` 静态库，源文件集中列一次，主程序和测试都链接它。

### 5.5 推送 GitHub 失败

**现象**：

```
Failed to connect to github.com:443 after 21123 ms: Could not connect to server
```

**排查过程**：

1. 测试发现百度能通，说明网络正常；
2. `github.com` 解析到 `20.247.166.64`，这个 IP 连不上；
3. 直连 `140.82.112.3`、`20.205.243.166` 等 GitHub 真实 IP 却能通；
4. 结论：运营商 DNS 返回了错误地址。

**解决**：把系统 DNS 改成 `223.5.5.5`，刷新缓存后推送成功。

---

## 6. 测试与验证

### 6.1 测试设计

测试程序在 `tests/test_main.cpp`，不依赖第三方框架，自己写了两个小工具：

```cpp
void check(bool ok, const std::string& name);   // 记录 PASS / FAIL
template <typename T, typename Func>
bool throwsAs(Func&& func);                     // 检查是否抛出指定类型的异常
```

### 6.2 测试结果

```
[PASS] 空消息内容抛 MessageException
[PASS] 消息内容读取正确
[PASS] 时间戳长度为 19（YYYY-MM-DD HH:MM:SS）
[PASS] 空节点名抛 NodeException
[PASS] 全空白节点名抛 NodeException
[PASS] 空话题名抛 TopicException
[PASS] 重复节点名抛 NodeException
[PASS] 话题下没有订阅者时抛 TopicException
[PASS] 订阅者注册数量为 1
[PASS] 正常发布不抛异常
[PASS] 统一关闭后注册表清空
[PASS] SensorMessage 温度字段正确
[PASS] SensorMessage 湿度字段正确
[PASS] toString 包含温度

通过: 14，失败: 0
```

### 6.3 演示程序运行结果（节选）

```
[INFO] ========== 系统启动 ==========
[INFO] [Node:pub_temp] 发布者初始化完成 topic=/room/temp
[INFO] [Node:pub_temp] 已注册为发布者 topic=/room/temp
[INFO] [Node:sub_temp_display] 订阅者初始化完成 topic=/room/temp
[INFO] [Node:sub_temp_app] 订阅者初始化完成 topic=/room/temp
...
[INFO] [Node:pub_temp] 发布消息 topic=/room/temp 内容=25.3
[INFO] [Node:sub_temp_display] 收到消息: 25.3 @ 2026-11-16 20:01:42
[INFO] [Node:sub_temp_app] 收到消息: 25.3 @ 2026-11-16 20:01:42
[INFO] [Node:pub_humid] 发布消息 topic=/room/humid 内容=60%
[INFO] [Node:sub_humid_display] 收到消息: 60% @ 2026-11-16 20:01:42
[INFO] [Node:sub_humid_app] 收到消息: 60% @ 2026-11-16 20:01:42
[INFO] [Node:pub_temp] 发布者已关闭
...
[INFO] ========== 系统关闭 ==========
```

从输出可以验证三点：话题隔离、一对多转发、完整生命周期。

---

## 7. 加分项实现说明

| 加分项 | 实现方式 | 状态 |
| --- | --- | --- |
| 日志持久化 | Logger 同时写控制台和 `logs/ros_sim.log` | ✅ |
| 全局异常处理 | main 分类捕获，返回退出码 | ✅ |
| 自定义消息 | SensorMessage 继承 Message，重写 toString | ✅ |
| 服务/客户端 | 未实现 | ❌ |
| 动态增删节点 | `ros_console` 运行时创建与删除，ROS 提供 unregister 接口 | ✅ |

---

## 8. 已知限制与改进方向

| 限制 | 说明 | 改进方向 |
| --- | --- | --- |
| 单线程 | 转发在发布调用栈内同步完成 | 加消息队列 + 工作线程 |
| `localtime` 非线程安全 | 多线程下会有数据竞争 | 改用 `localtime_s` / `localtime_r` |
| 日志无轮转 | 文件会一直变大 | 按大小或日期切分 |
| 无服务/客户端 | 只有发布订阅 | 增加 Service/Client 类与注册表 |
| 状态文件为自定义格式 | 手工修改容易出错 | 改用 JSON 或 SQLite |
| 无序列化 | 消息只在进程内传递 | 增加序列化层以支持网络通信 |

---

## 9. 总结

这个项目规模不大，但把 C++ 里几个容易出错的地方都踩了一遍：对象所有权、虚函数与多态、异常安全、对象切片、编码问题。

代码量约 1300 行，测试 14 条全部通过。最终实现覆盖了课程要求的全部必做项，以及日志持久化、全局异常处理、自定义消息、动态增删节点四个加分方向。
