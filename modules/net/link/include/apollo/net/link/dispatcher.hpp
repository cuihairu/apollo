#pragma once

// 按 id 分发器（net M1 内核 · 增量 2；sdk-contract §10.6 附节：C++ 帧路由层
// 的机制件——msg id → 路由表查询 → 域过滤 → handler）。
//
// 机制/策略分界：白名单与 handler 是装配策略（Link 配置注入），分发与
// 记账是本类机制。未注册 id、域越权、无 handler 三类拒收全部计数上报，
// 不静默、不抛异常（正确性内核纪律：错误是数据不是控制流）。

#include "apollo/net/link/route_table.hpp"

#include <cstdint>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace apollo::net::link {

enum class DispatchResult {
    kDispatched,       // 命中 handler，已投递
    kUnknownId,        // 路由表无此 id（非契约消息——拒）
    kDomainForbidden,  // 域越权（上行白名单过滤——拒）
    kNoHandler,        // 表内有、本进程未装配 handler（计数，回调兜底）
};

class Dispatcher {
public:
    // 载荷视图（id 已剥出；data 不含 id 前缀，可空）。
    using Handler = std::function<void(std::uint16_t id, const std::uint8_t* data,
                                       std::size_t size)>;

    // 路由表（进程一份，装配期注入；不持所有权）。
    void set_route_table(const RouteTable* table) { table_ = table; }

    // 上行域白名单（§11.3 域过滤）：空集 = 不过滤（机制默认开放，
    // 策略由装配收口——链路配置决定给哪些域过闸）。
    void allow_domain(std::string domain) { allowed_.insert(std::move(domain)); }
    void clear_domains() { allowed_.clear(); }

    void set_handler(std::uint16_t id, Handler handler) {
        handlers_[id] = std::move(handler);
    }
    // 兜底（无 handler 时的观测/透传口；缺省无操作）。返回 false 可令
    // 该帧按 kNoHandler 计数（默认）——兜底自己消费时返回 true。
    void set_fallback(Handler fallback) { fallback_ = std::move(fallback); }

    // id 解出后的分发入口。载荷非法性（字段级）归各 handler 的按型解码。
    DispatchResult dispatch(std::uint16_t id, const std::uint8_t* data, std::size_t size);

    struct Stats {
        std::uint64_t dispatched = 0;
        std::uint64_t unknown_id = 0;
        std::uint64_t domain_forbidden = 0;
        std::uint64_t no_handler = 0;
    };
    const Stats& stats() const { return stats_; }

private:
    const RouteTable* table_ = nullptr;
    std::unordered_set<std::string> allowed_;
    std::unordered_map<std::uint16_t, Handler> handlers_;
    Handler fallback_;
    Stats stats_;
};

}  // namespace apollo::net::link
