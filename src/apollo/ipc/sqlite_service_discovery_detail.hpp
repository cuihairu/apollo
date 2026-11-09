#pragma once

#include <map>
#include <string>

namespace apollo {
namespace ipc {

// sqlite_service_discovery 拆分单元共享序列化小工具（单定义在
// sqlite_service_discovery.cpp——原匿名域上收为命名空间级声明，
// 各拆分 TU 调用点不限定名保持不变）
std::string metadataToJson(const std::map<std::string, std::string>& metadata);
std::map<std::string, std::string> jsonToMetadata(const std::string& json);

} // namespace ipc
} // namespace apollo
