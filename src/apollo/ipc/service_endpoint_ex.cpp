/**
 * @file service_endpoint_ex.cpp
 * @brief 扩展服务端点实现
 */

#include "apollo/ipc/service_endpoint_ex.h"
#include <sstream>
#include <algorithm>

namespace apollo {
namespace ipc {

//==============================================================================
// ServiceEndpointEx 序列化
//==============================================================================

std::string ServiceEndpointEx::toJsonEx() const {
    std::ostringstream oss;

    oss << "{"
        << "\"id\":\"" << id << "\","
        << "\"host\":\"" << host << "\","
        << "\"port\":" << port << ","
        << "\"protocol\":\"" << protocol << "\","
        << "\"healthy\":" << (healthy ? "true" : "false") << ","
        << "\"weight\":" << weight << ","
        << "\"lastHeartbeat\":" << lastHeartbeat << ","

        // 扩展字段
        << "\"transportMask\":" << static_cast<uint8_t>(transportMask) << ","

        // 地址信息
        << "\"shmAddress\":{"
        << "\"serverHost\":\"" << shmAddress.serverHost << "\","
        << "\"serverPort\":" << shmAddress.serverPort << ","
        << "\"protocol\":\"" << shmAddress.protocol << "\","
        << "\"unixPath\":\"" << shmAddress.unixPath << "\","
        << "\"shmName\":\"" << shmAddress.shmName << "\""
        << "},"

        << "\"unixAddress\":{"
        << "\"serverHost\":\"" << unixAddress.serverHost << "\","
        << "\"serverPort\":" << unixAddress.serverPort << ","
        << "\"protocol\":\"" << unixAddress.protocol << "\","
        << "\"unixPath\":\"" << unixAddress.unixPath << "\""
        << "},"

        << "\"tcpAddress\":{"
        << "\"serverHost\":\"" << tcpAddress.serverHost << "\","
        << "\"serverPort\":" << tcpAddress.serverPort << ","
        << "\"protocol\":\"" << tcpAddress.protocol << "\""
        << "},"

        // 部署信息
        << "\"pid\":\"" << pid << "\","
        << "\"startTime\":" << startTime << ","
        << "\"dataCenter\":\"" << dataCenter << "\","
        << "\"rack\":\"" << rack << "\","
        << "\"hostId\":" << static_cast<int>(hostId) << ","
        << "\"priority\":" << static_cast<int>(priority) << ","

        // 元数据
        << "\"metadata\":{";

    bool first = true;
    for (const auto& [k, v] : metadata) {
        if (!first) oss << ",";
        oss << "\"" << k << "\":\"" << v << "\"";
        first = false;
    }

    oss << "}}";
    return oss.str();
}

ServiceEndpointEx ServiceEndpointEx::fromJsonEx(const std::string& json) {
    ServiceEndpointEx ex;

    // 简化版解析，生产环境应使用 nlohmann/json
    // TODO: 完整 JSON 解析

    // 从基础 ServiceEndpoint 解析
    size_t pos;

    if ((pos = json.find("\"id\":")) != std::string::npos) {
        size_t start = json.find("\"", pos + 5) + 1;
        size_t end = json.find("\"", start);
        ex.id = json.substr(start, end - start);
    }

    if ((pos = json.find("\"host\":")) != std::string::npos) {
        size_t start = json.find("\"", pos + 7) + 1;
        size_t end = json.find("\"", start);
        ex.host = json.substr(start, end - start);
    }

    if ((pos = json.find("\"port\":")) != std::string::npos) {
        size_t start = pos + 7;
        size_t end = json.find(",", start);
        if (end == std::string::npos) end = json.find("}", start);
        ex.port = static_cast<uint16_t>(std::stoi(json.substr(start, end - start)));
    }

    return ex;
}

//==============================================================================
// LocalFirstSelector 实现
//==============================================================================

ServiceEndpointEx LocalFirstSelector::select(
    const std::vector<ServiceEndpointEx>& endpoints) const {

    if (endpoints.empty()) return {};

    // 先过滤本地可达的
    auto localEndpoints = filterLocal(endpoints);

    // 如果有本地端点，优先选择
    if (!localEndpoints.empty()) {
        // 按优先级排序后选择第一个
        auto sorted = sortByPriority(std::move(localEndpoints));

        // 在同优先级中随机选择
        if (sorted.size() > 1) {
            static thread_local std::random_device rd;
            static thread_local std::mt19937 gen(rd());
            std::uniform_int_distribution<size_t> dist(0,
                std::min(sorted.size(), size_t(3)) - 1);  // 从前3个中随机
            return sorted[dist(gen)];
        }
        return sorted[0];
    }

    // 没有本地端点，返回第一个远程端点（已按优先级排序）
    auto sorted = sortByPriority(endpoints);
    return sorted[0];
}

std::vector<ServiceEndpointEx> LocalFirstSelector::filterLocal(
    const std::vector<ServiceEndpointEx>& endpoints) const {

    std::vector<ServiceEndpointEx> result;

    for (const auto& ep : endpoints) {
        // 检查是否同一数据中心和主机
        bool sameDataCenter = (config_.localDataCenter == "0" ||
                               config_.localDataCenter == ep.dataCenter);

        bool sameHost = (config_.localHost == "0" ||
                         config_.localHost == ep.host ||
                         config_.localHost == ep.tcpAddress.serverHost);

        if (sameDataCenter && sameHost) {
            result.push_back(ep);
        }
    }

    return result;
}

std::vector<ServiceEndpointEx> LocalFirstSelector::sortByPriority(
    std::vector<ServiceEndpointEx> endpoints) const {

    std::sort(endpoints.begin(), endpoints.end(),
        [](const ServiceEndpointEx& a, const ServiceEndpointEx& b) {
            // 按优先级排序，数字越小优先级越高
            return static_cast<int>(a.priority) < static_cast<int>(b.priority);
        });

    return endpoints;
}

} // namespace ipc
} // namespace apollo
