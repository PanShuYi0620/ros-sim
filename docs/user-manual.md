# 用户手册

本手册面向第一次使用本项目的人，介绍如何构建、运行和排查问题。

---

## 1. 这是什么

`ros_sim` 是一个用 C++ 写的简易 ROS 通信模拟程序。它模拟了 ROS 最核心的「发布者—话题—订阅者」模型：

- 发布者往某个话题发消息
- 订阅者订阅该话题，自动收到消息
- 中间由一个单例的通信核心负责转发

程序启动后会创建几个演示节点，跑一遍完整流程：初始化 → 收发消息 → 统一关闭。

---

## 2. 环境要求

| 项 | 要求 |
| --- | --- |
| 操作系统 | Windows 10/11，Linux 也可编译 |
| 编译器 | 支持 C++17（MSVC 2022+ / GCC 9+ / Clang 10+） |
| 构建工具 | CMake 3.16 及以上 |
| 第三方库 | 不需要 |

**推荐环境**：CLion 2026（自带 MinGW 和 CMake，装好即可用）。

> 注意：项目路径**不要包含中文**，否则 CLion 的调试器无法加载程序。如果用户名是中文，可以把项目放到 `C:\Users\Public\` 这类纯英文路径下。

---

## 3. 获取代码

```bash
git clone https://github.com/PanShuYi0620/ros-sim.git
```

或者直接从课程提交的仓库地址下载压缩包。

---

## 4. 构建

### 方式一：用 CLion（推荐）

1. 打开 CLion
2. **File → Open**，选择项目文件夹（含 `CMakeLists.txt` 的那一层）
3. 弹出信任提示点 **Trust**
4. 等待右下角 CMake 加载完成
5. 点右上角 **锤子图标** 编译

编译成功后，目标程序在 `cmake-build-debug/` 目录下。

### 方式二：命令行

```bash
cmake -S . -B build
cmake --build build
```

Windows 下产物是 `build/ros_sim.exe` 和 `build/ros_tests.exe`。

---

## 5. 运行演示程序

### 在 CLion 里

1. 右上角运行配置选择 **`ros_sim`**
2. 点绿色三角 ▶️

### 在命令行

```bash
./build/ros_sim
```

Windows 下如果双击 exe 提示缺少 DLL，是因为没把 MinGW 的 `bin` 目录加入 PATH；在 CLion 里运行不会有这个问题。

### 预期输出

```
[2026-11-16 20:01:42] [INFO] ========== 系统启动 ==========
[2026-11-16 20:01:42] [INFO] [Node:pub_temp] 发布者初始化完成 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:pub_temp] 已注册为发布者 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_display] 订阅者初始化完成 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_display] 已注册为订阅者 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_app] 订阅者初始化完成 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_app] 已注册为订阅者 topic=/room/temp
[2026-11-16 20:01:42] [INFO] [Node:pub_humid] 发布者初始化完成 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:pub_humid] 已注册为发布者 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:sub_humid_display] 订阅者初始化完成 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:sub_humid_display] 已注册为订阅者 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:sub_humid_app] 订阅者初始化完成 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:sub_humid_app] 已注册为订阅者 topic=/room/humid
[2026-11-16 20:01:42] [INFO] [Node:pub_temp] 发布消息 topic=/room/temp 内容=25.3
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_display] 收到消息: 25.3 @ 2026-11-16 20:01:42
[2026-11-16 20:01:42] [INFO] [Node:sub_temp_app] 收到消息: 25.3 @ 2026-11-16 20:01:42
...
[2026-11-16 20:01:42] [INFO] [Node:pub_bad] 发布消息 topic=/room/none 内容=这条一定会失败
[2026-11-16 20:01:42] [ERROR] [Topic] 话题不存在或没有订阅者: /room/none
```

**怎么看这段输出**

| 看什么 | 说明 |
| --- | --- |
| 初始化日志 | 每个节点创建后都会打一条 |
| 注册日志 | 节点注册到 ROS 核心后打一条 |
| 发布 + 接收 | 一条消息被该话题下所有订阅者收到 |
| 话题隔离 | `/room/temp` 的消息不会跑到 `/room/humid` 的订阅者那里 |
| 最后一行 ERROR | 故意向无订阅者的话题发布，验证异常被捕获，程序没有崩溃 |

最后一行为 `[ERROR]` 是**预期行为**，不是程序出错。它证明程序遇到非法操作时能优雅处理。

---

## 6. 运行测试

### 在 CLion 里

1. 右上角运行配置切换到 **`ros_tests`**
2. 点绿色三角

### 在命令行

```bash
./build/ros_tests
```

### 预期输出

```
[PASS] 空消息内容抛 MessageException
[PASS] 消息内容读取正确
...
通过: 14，失败: 0
```

看到 `失败: 0` 和退出码 0 就说明全部通过。

---

## 7. 交互式控制台 ros_console

除了自动演示，项目还提供了交互式控制台，可以在运行时自己创建节点、发消息、查看结果。

### 启动

运行配置切换到 **`ros_console`**，或在命令行执行：

```bash
./build/ros_console
```

启动后出现 `> ` 提示符，等待输入命令。

### 命令表

| 命令 | 作用 |
| --- | --- |
| `pub <名字>` | 创建发布者（尚未绑定话题） |
| `sub <名字>` | 创建订阅者（尚未订阅话题） |
| `topic <发布者> <话题>` | 由发布者创建新话题 |
| `join <发布者> <话题>` | 发布者绑定到已存在的话题 |
| `listen <订阅者> <话题>` | 订阅者订阅已存在的话题 |
| `send <发布者> <话题> <内容>` | 向指定话题发消息 |
| `recv <订阅者>` | 查看该订阅者收到的消息 |
| `del pub\|sub\|topic <名字>` | 删除发布者 / 订阅者 / 话题 |
| `info <话题>` | 查看话题详情（含已注销发布者） |
| `clear` | 清空所有话题与节点 |
| `list` | 查看当前状态 |
| `help` | 显示帮助 |
| `exit` | 保存并退出 |

### 示例流程

```text
> pub p1
发布者 p1 已创建（尚未绑定话题）

> topic p1 /room/temp
话题 /room/temp 已创建，创建者 p1

> sub s1
订阅者 s1 已创建（尚未订阅话题）

> listen s1 /room/temp
订阅者 s1 已订阅话题 /room/temp

> send p1 /room/temp 25.3
已发送到 /room/temp，1 个订阅者收到

> recv s1
订阅者 s1 收到 1 条消息:
  25.3 @ 2026-11-17 21:30:00

> exit
已退出（状态已保存，下次启动会恢复）
```

### 状态保存

控制台会把状态写入项目根目录的 `console_state.txt`，包括话题、发布者、订阅者和已收到的消息。下次启动自动恢复，可以接着操作。

每条命令执行后都会保存一次，所以直接关闭窗口也不会丢失。想清空重来，删除这个文件或在控制台输入 `clear`。

> 控制台只显示交互内容，系统日志仍然写入 `logs/ros_sim.log`。

---

## 8. 日志文件

程序运行时会自动在项目目录下创建：

```
logs/ros_sim.log
```

内容是控制台输出的完整副本，采用追加模式，多次运行不会覆盖。可以直接用记事本或 CLion 打开查看。

---

## 9. 常见问题

### Q1：编译时报错“找不到 ros/xxx.hpp”

**原因**：CMake 没有加载成功，或打开的不是项目根目录。

**解决**：确认打开的是包含 `CMakeLists.txt` 的那一层，然后执行 **Tools → CMake → Reload CMake Project**。

### Q2：新增了 .cpp 文件，但改动不生效

**原因**：新文件没加进 `CMakeLists.txt`。

**解决**：把文件名加到 `add_library(ros_core ...)` 的列表里，然后 Reload CMake。

### Q3：CLion 调试时提示 “non-ASCII characters in the path”

**原因**：项目路径里有中文（比如用户名是中文）。

**解决**：把项目放到纯英文路径下，或把 CMake 构建目录改到纯英文路径。

### Q4：控制台中文乱码

**原因**：Windows 控制台默认用 GBK 解码 UTF-8 输出。

**解决**：程序里已经调用 `SetConsoleOutputCP(CP_UTF8)`；如果仍然乱码，先执行 `chcp 65001`，或改用 CLion 的终端。

### Q5：运行提示缺少 DLL

**原因**：MinGW 编译的程序需要 `libstdc++-6.dll` 等运行库。

**解决**：在 CLion 里运行（会自动配好环境），或把 MinGW 的 `bin` 目录加入系统 PATH。

### Q6：`git push` 连接超时

**原因**：DNS 把 `github.com` 解析到了连不上的地址。

**解决**：把系统 DNS 改成 `223.5.5.5`，执行 `ipconfig /flushdns` 后重试。

---

## 10. 清理

项目里的构建产物和日志都可以安全删除，不影响源码：

```
build/               编译产物
cmake-build-debug/   CLion 构建目录
logs/                运行日志
```

删除后重新构建即可。
