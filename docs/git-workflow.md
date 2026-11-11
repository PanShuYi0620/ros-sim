# Git 日常工作流速查

> 本项目最小操作集。所有命令都在项目根目录（`ros-sim/`）下执行。

## 一、一次性配置（做完就不用再管）

```bash
git config --global user.name "你的名字"
git config --global user.email "你的邮箱"
git config --global init.defaultBranch main
```

## 二、日常四步

```bash
git status                          # 1. 看改了什么
git add .                           # 2. 暂存所有改动
git commit -m "feat: 实现 Message 类"  # 3. 提交
git push                            # 4. 推到 GitHub
```

## 三、提交信息怎么写

| 前缀 | 用途 | 例子 |
| --- | --- | --- |
| feat | 新功能 | `feat: 实现 Publisher 发布消息` |
| fix | 修 bug | `fix: 修复重复节点名未拦截` |
| docs | 文档 | `docs: 添加需求文档` |
| test | 测试 | `test: 补充异常场景用例` |
| chore | 杂项 | `chore: 初始化项目骨架` |

## 四、查看与撤销

```bash
git log --oneline --graph          # 查看提交历史
git diff                           # 查看未暂存的改动
git restore 文件名                  # 丢弃某个文件的未暂存修改
git restore --staged 文件名         # 取消暂存
git commit --amend -m "新信息"      # 修改最近一次提交（push 之前）
```

## 五、本项目的提交节奏

1. `chore: 初始化项目骨架`
2. `docs: 添加需求文档`
3. `docs: 添加架构设计文档`
4. `feat: 实现 Message 与时间戳`
5. `feat: 实现 Node 抽象基类`
6. `feat: 实现 ROS 单例与注册表`
7. `feat: 实现 Publisher`
8. `feat: 实现 Subscriber 与消息转发`
9. `feat: 添加多节点生命周期示例`
10. `fix: 补齐空名称 / 重复注册 / 空消息等异常`
11. `test: 添加结构化测试用例`
12. `docs: 补全技术文档与用户手册`

> 原则：**一次提交只做一件事，且提交后代码仍然能编译**。
