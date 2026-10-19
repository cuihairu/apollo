#pragma once

// Base 唯一激活/写权登记（todo L64 批⑰）——「玩家当前 Base + 归属代次」的单一权威。
//
// 一个玩家同一时刻只有一个持权 Base：activate() 单调分配归属代次，后到顶替先到
// （并发登录：新会话 Base 接管，旧 Base 的迟到写权即刻失效）；resolve() 是
// OrderDelivery::BaseResolver 缝的真实来源（nullopt = 离线/无主/迁移中）；
// release() 仅当代次匹配当前持有者——被顶替的旧 Base 迟到释放不动新归属。
// 激活与释放均不接触 PlayerDirectory/会话面（离线激活即登记，登记即生效）。
//
// journal 文本行（append-only 事件序，5 字段统一格式）：
//   state|player_id|generation|timestamp_ms|address
//   state ∈ {A=激活, R=释放}；R 行地址字段为空（释放不指向 Base）。回放重建
//   当前持有者 + 每玩家代次水位：重启不回退代次（旧进程拿不到已发号代次），
//   故归属代次在接口/存储重启后仍是有效的校验凭据。释放事件不降水位。
//
// 线程模型：类内互斥（激活面/投递面两线程安全）；resolve() 返回拷贝。

#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <optional>
#include <string>

#include "apollo/game/orders/order_delivery.hpp"  // BaseEndpoint

namespace apollo::game::orders {

struct AuthoritySnapshot {
    std::uint64_t generation = 0;  // 归属代次（0 = 激活被拒/未知）
    std::string address;            // 持权 Base 地址
    std::int64_t since_ms = 0;      // 本代激活时刻
};

class BaseAuthorityRegistry {
public:
    explicit BaseAuthorityRegistry(std::string journal_path);

    // 打开/恢复：回放重建当前持有者与代次水位；目录缺失/不可写 → false。
    bool open();

    // 激活/接管：分配新代次并顶替既有持有者。空玩家/空地址 → 空快照（不写盘）。
    AuthoritySnapshot activate(std::uint64_t player_id, const std::string& address,
                               std::int64_t now_ms);

    // 当前写权归属（BaseResolver 缝来源）：nullopt = 离线/无主。
    std::optional<BaseEndpoint> resolve(std::uint64_t player_id) const;

    // 释放：仅当代次匹配当前持有者（被顶替旧 Base 迟到释放无效）。
    bool release(std::uint64_t player_id, std::uint64_t generation);

    std::optional<AuthoritySnapshot> holder(std::uint64_t player_id) const;  // 观测面
    std::size_t holder_count() const;

private:
    std::string render_line(char state, std::uint64_t player_id, std::uint64_t generation,
                            std::int64_t ms, const std::string& address) const;
    bool append_line(const std::string& line);
    bool replay();
    std::uint64_t next_generation(std::uint64_t player_id) const;  // 锁内调用

    mutable std::mutex mutex_;
    std::string journal_path_;
    std::map<std::uint64_t, AuthoritySnapshot> holders_;   // 当前持权
    std::map<std::uint64_t, std::uint64_t> generation_;   // 每玩家代次水位
    std::ofstream out_;
    bool opened_ = false;
};

}  // namespace apollo::game::orders
