#pragma once

#include <memory>
#include <string>

namespace ros_sim {

// 所有节点的抽象基类：只负责“名字 + 生命周期契约”
class Node : public std::enable_shared_from_this<Node> {
  public:
    explicit Node(std::string name); // 空名称抛 NodeException
    virtual ~Node() = default;

    const std::string& name() const noexcept {
        return name_;
    }

    virtual void init() = 0;
    virtual void shutdown() = 0;

  protected:
    static std::string validateName(std::string name);

  private:
    std::string name_;
};

} // namespace ros_sim
