#pragma once

// 地址字符串 helper（P4-4 批⑤：nng 退役后从 nng_wrapper.hpp 抽出的纯
// 字符串面——无传输语义，编址格式两处消费面共用）。

#include <cstdint>
#include <string>

namespace apollo::protocol {

// 创建地址字符串
inline std::string make_url(const std::string& scheme, const std::string& host, uint16_t port) {
    return scheme + "://" + host + ":" + std::to_string(port);
}

inline std::string make_tcp_url(const std::string& host, uint16_t port) {
    return make_url("tcp", host, port);
}

inline std::string make_ipc_url(const std::string& path) {
    return "ipc://" + path;
}

inline std::string make_inproc_url(const std::string& name) {
    return "inproc://" + name;
}

} // namespace apollo::protocol
