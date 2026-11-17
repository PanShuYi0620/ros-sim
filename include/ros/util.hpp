#pragma once

#include <string>

namespace ros_sim {

// 生成 "YYYY-MM-DD HH:MM:SS" 格式的当前本地时间
std::string nowTimestamp();

// 校验话题名：空或全为空白时抛 TopicException，正常时原样返回
std::string validateTopicName(std::string topic);

} // namespace ros_sim
