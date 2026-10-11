#pragma once

// 日志 push 客户端（L4：logging.md §5 collector 形态的发送面——各进程
// logger 双出口的第二出口传输适配器；ADR-013「push 留接口」的落地面）。
//
// 断连降级（logging.md §5 失败语义）：内部有界队列缓冲（collector 挂 =
// 只写本地），重连后按序补投；队列顶格丢新计数（本地文件恒为真相源，
// 丢推不丢账）。收集永不构成运行依赖：push 不阻塞、不抛、零业务反馈。
//
// 线程纪律：push 供 logger 写路径任意线程调用（互斥入队即回）；链路
// 收发/重连/补投全在内部泵线程（单写者）。链路面 = InterServerLink
// active 端点 → collector（apps/logger serve）。
//
// Windows：link TCP 底座不进本批（POSIX only）——退空件（push 恒落地，
// 本地文件照写），调用面无 #if。

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <string>

#if !defined(_WIN32)
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_table.hpp"
#include <thread>
#endif

namespace apollo::net::logpush {

struct LogPushClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 9102;    // collector serve 监听口
    std::string route_json;       // 契约路由 JSON 路径（空 = start 失败显形）
    std::string self_name;        // 本进程组件名（握手身份 = proc 键同源）
    std::size_t queue_cap = 4096; // 断连缓冲上限（顶格丢新）
    std::uint32_t pump_interval_ms = 50;
};

class LogPushClient {
public:
    explicit LogPushClient(LogPushClientConfig config);
    ~LogPushClient();

    LogPushClient(const LogPushClient&) = delete;
    LogPushClient& operator=(const LogPushClient&) = delete;

    // 装配（路由表缺失/坏路径 = false 降级为本地-only，不 throw）
    bool start();
    void stop();

    // logger 写路径调用面：入队即回（顶格丢新 = false）。行内容 = 结构化
    // 行原文（六键 logfmt，structured.h 契约）。
    bool push(std::string structured_line);

    // 观察面
    bool connected() const;
    std::uint64_t pushed() const;   // 已被链路受理（含断连排队）
    std::uint64_t dropped() const;  // 顶格拒收
    std::size_t queued() const;

private:
    void run();

    LogPushClientConfig config_;
    std::deque<std::string> queue_;
    mutable std::mutex queue_mutex_;  // queued() 观测面 const 锁
    std::atomic<bool> running_{false};
    std::atomic<std::uint64_t> pushed_{0};
    std::atomic<std::uint64_t> dropped_{0};

#if !defined(_WIN32)
    std::unique_ptr<link::RouteTable> route_table_;
    std::unique_ptr<link::LinkManager> manager_;
    std::thread pump_thread_;
#endif
};

} // namespace apollo::net::logpush
