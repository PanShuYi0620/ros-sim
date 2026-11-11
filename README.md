# ros_sim —— C++ 模拟 ROS 节点通信系统

> C++ 课程大作业：不依赖真实 ROS，用 C++17 标准库实现一个迷你版 ROS 通信内核。

## 项目简介

用单例的 ROS 核心维护「话题 → 发布者」与「话题 → 订阅者」两张注册表，
发布者把消息投递到话题，核心再把消息分发给该话题下的所有订阅者，
从而模拟 ROS 中节点—话题的松耦合通信。

## 功能规划

- [x] 项目骨架（CMake + 目录结构）
- [ ] `Message`：消息内容 + 自动时间戳（`YYYY-MM-DD HH:MM:SS`）
- [ ] `Node`：抽象基类，唯一名称、`init()` / `shutdown()` 纯虚接口
- [ ] `Publisher`：向话题发布消息
- [ ] `Subscriber`：接收并打印消息
- [ ] `ROS`：单例，注册 / 转发 / 统一关闭
- [ ] 异常处理：空节点名、重复注册、空消息、话题不存在
- [ ] 日志分级与落盘（加分）
- [ ] 自定义消息类型（加分）
- [ ] 服务 / 客户端同步通信（加分）
- [ ] 动态增删节点（加分）
- [ ] 结构化测试用例

## 构建

要求：CMake ≥ 3.16，支持 C++17 的编译器（MSVC 2019+ / GCC 9+ / Clang 10+）。

```bash
cmake -S . -B build
cmake --build build --config Release
# Windows 下运行 build/Release/ros_sim.exe，单配置生成器为 build/ros_sim.exe
```

CLion 用户直接 **Open** 本目录即可，无需额外配置。

> 提示：在普通终端直接运行 MinGW 构建的 exe 若报 `0xc0000135`（缺 DLL），
> 把 CLion 的 `bin\mingw\bin` 加入 PATH，或链接时加 `-static` 即可。

## 目录结构

```
ros-sim/
├── CMakeLists.txt
├── include/ros/        # 头文件
├── src/                # 源文件
├── tests/              # 测试
├── docs/               # 文档
└── README.md
```

## 文档

| 文档 | 说明 |
| --- | --- |
| `docs/requirements.md` | 需求文档（编码前编写） |
| `docs/architecture.md` | 架构设计文档（编码前编写） |
| `docs/technical.md` | 技术文档（编码后编写） |
| `docs/user-manual.md` | 用户手册（编码后编写） |
