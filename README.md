# ros_sim —— C++ 模拟 ROS 节点通信系统

> C++ 课程大作业：不依赖真实 ROS，用 C++17 标准库实现一个迷你版 ROS 通信内核。

## 项目简介

用单例的 ROS 核心维护「话题 → 发布者」与「话题 → 订阅者」两张注册表。发布者把消息投递到话题，核心再分发给该话题下的所有订阅者，模拟 ROS 中节点—话题的松耦合通信。

整个项目只使用 C++ 标准库，不依赖任何第三方库。

## 功能

- [x] `Node` 抽象基类：唯一名称、`init()` / `shutdown()` 纯虚接口
- [x] `Publisher`：向话题发布消息、注册、日志
- [x] `Subscriber`：接收消息、消息箱、日志
- [x] `Message`：内容 + 自动时间戳（`YYYY-MM-DD HH:MM:SS`），只读
- [x] `ROS` 单例：两张注册表、名称查重、消息转发、统一关闭、注销
- [x] 异常处理：空节点名、空话题名、空消息、节点重名、话题不存在
- [x] 日志分级与落盘（加分）
- [x] 全局异常捕获（加分）
- [x] 自定义消息 `SensorMessage`（加分）
- [x] 运行时动态增删节点（加分，`ros_console`）
- [x] 结构化测试：14 条用例，覆盖正常 / 边界 / 异常
- [ ] 服务 / 客户端同步通信（未实现）

## 三个可执行程序

| 程序 | 说明 |
| --- | --- |
| `ros_sim` | 静态演示：多话题、多订阅者的完整生命周期 |
| `ros_tests` | 测试程序：14 条用例，退出码 0 表示全部通过 |
| `ros_console` | 交互式控制台：运行时创建 / 删除节点、发消息、状态持久化 |

### ros_console 命令

| 命令 | 作用 |
| --- | --- |
| `pub <名字>` | 创建发布者（尚未绑定话题） |
| `sub <名字>` | 创建订阅者（尚未订阅话题） |
| `topic <发布者> <话题>` | 创建新话题并绑定 |
| `join <发布者> <话题>` | 绑定到已存在的话题 |
| `listen <订阅者> <话题>` | 订阅已存在的话题 |
| `send <发布者> <话题> <内容>` | 向指定话题发消息 |
| `recv <订阅者>` | 查看该订阅者收到的消息 |
| `del pub\|sub\|topic <名字> [话题]` | 删除发布者 / 订阅者 / 话题（`del pub p1 /t` 只解绑一个话题） |
| `info <话题>` | 查看话题详情（含已注销发布者） |
| `history` | 查看最近执行的命令 |
| `clear` | 清空所有话题与节点 |
| `list` | 查看当前状态 |
| `help` | 显示帮助 |
| `exit` | 保存并退出 |

## 构建

要求：CMake ≥ 3.16，支持 C++17 的编译器（MSVC 2022+ / GCC 9+ / Clang 10+）。

```bash
cmake -S . -B build
cmake --build build
```

CLion 用户直接 **File → Open** 本目录即可，CLion 会自动加载 CMake。

> 提示：路径尽量不要包含中文，否则 CLion 自带的 GDB 可能无法加载程序。
> 如果直接运行 MinGW 构建的 exe 报 `0xc0000135`（缺 DLL），把 MinGW 的 `bin` 目录加入 PATH 即可。

## 运行

| 程序 | 在 CLion 里 | 命令行 |
| --- | --- | --- |
| 演示 | 选择 `ros_sim` → 绿色三角 | `./build/ros_sim` |
| 测试 | 选择 `ros_tests` → 绿色三角 | `./build/ros_tests` |
| 控制台 | 选择 `ros_console` → 绿色三角 | `./build/ros_console` |

演示程序最后会输出一行 `[ERROR]`，那是**故意**向无订阅者的话题发布消息，用来验证异常被捕获、程序不崩溃。

## 目录结构

```
ros-sim/
├── CMakeLists.txt
├── include/ros/        # 头文件（声明）
├── src/                # 源文件（实现）
├── tests/              # 测试
├── docs/               # 文档
├── logs/               # 运行日志（自动生成）
└── README.md
```

## 文档

| 文档 | 说明 |
| --- | --- |
| `docs/requirements.md` | 需求文档（编码前编写） |
| `docs/architecture.md` | 架构设计文档（编码前编写） |
| `docs/technical.md` | 技术文档（编码后编写） |
| `docs/user-manual.md` | 用户手册（编码后编写） |

## 技术要点

- **单例**：ROS 与 Logger 都用 Meyers 写法，线程安全、无手动释放
- **对象所有权**：节点由 `shared_ptr` 管理，ROS 持有强引用
- **多态**：`Message::toString()` 为虚函数，`SensorMessage` 重写后自动生效
- **防切片**：消息一律以 `const Message&` 传递
- **异常分层**：`RosException` → `NodeException` / `TopicException` / `MessageException`
- **状态持久化**：`ros_console` 把状态写入 `console_state.txt`，重启可恢复
