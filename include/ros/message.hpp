#pragma once

#include <string>

namespace ros_sim {

class Message {
public:
    explicit Message(std::string content);
    virtual ~Message() = default;

    const std::string& content() const noexcept{ return content_; }
    const std::string& timestamp() const noexcept{ return timestamp_; }

    virtual std::string toString() const;

protected:
    static std::string makeTimestamp();

private:
    std::string content_;
    std::string timestamp_;
};

}  // namespace ros_sim

