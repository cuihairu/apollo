#pragma once

// 结算去重凭据库（P2-4「奖励与结算去重凭据持久提交」）：账本内存标记之外
// 的持久面——每条落账先提交凭据再入锚点，进程重启后凭据可装载回填账本，
// 重放不双记、终局可回收。
//
// 目录 = World 数据域下的 settlements/；每小队一份 settlement_<party>.cred，
// 行式 "<player> <score>"（人类可查、解析零依赖）；写 = tmp + rename 原子
// 替换（Windows 覆盖 rename 对已存在目标必败，先删后替）。生命周期：落账
// 提交 → 终局 teardown erase → 重启 ingest 装载 → 重新开账时回写。
// 并发口径与账本一致（单线程落账域，串行化随后续条目）。

#include <cstdint>
#include <string>
#include <unordered_map>

namespace squad_instance {

class SettlementCredentialStore {
public:
    // 小队 → (玩家 → 已落账分值)
    using Marks = std::unordered_map<std::uint64_t,
                                     std::unordered_map<std::uint64_t, std::int64_t>>;

    explicit SettlementCredentialStore(std::string dir);

    // 幂等提交：同 (小队, 玩家) 重复提交同值无副作用。
    // false = 未持久提交（目录不可造/写入失败）——账本据此挂账不落。
    bool record_applied(std::uint64_t party_id, std::uint64_t player_id,
                        std::int64_t score);

    // 凭据回收：副本终态收尾时调用。无此凭据亦返回 true（凭据已不在）。
    bool erase(std::uint64_t party_id);

    // 全量装载（重启面）：脏行/未知扩展名跳过，可解析行保留。
    [[nodiscard]] Marks load_all() const;

private:
    [[nodiscard]] std::string path_for(std::uint64_t party_id) const;
    [[nodiscard]] bool write_marks(std::uint64_t party_id,
                                   const std::unordered_map<std::uint64_t, std::int64_t>& marks) const;

    std::string dir_;
};

}  // namespace squad_instance
