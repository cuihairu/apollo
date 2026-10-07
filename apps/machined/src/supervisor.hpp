#pragma once

// Machined 监督面（P3-1 批 F——term-contract §1.3「拉起/重启」半边；§7 口径：
// 进程本身拉起归 machined，重启策略 = 编队配置，mgr 只消费事件不做进程管理）。
//
// 最小闭环：roster 花名册（name | command args...）→ 启动期全量拉起 → 主循环
// WNOHANG 收割死亡 → 按上限+线性退避自动重启（超限放弃并记录）。死亡/重生
// 事件以行文本落日志（跨进程上报归 manager 域消费端，G-1 主体后续批）。
//
// 平台口径：POSIX only（fork/execvp/waitpid）——部署目标是 Linux 服务器；
// Windows 下本文件编译为空转（roster 载入即告警放弃，进程发现面照常）。
// roster 口径：每行 `name | command args...`，# 注释、空行跳过；不支持引号
// 转义（骨架期，命令面从简）。

#include <cstdint>
#include <string>
#include <vector>

namespace machined {

struct RosterEntry {
    std::string name;
    std::string command;              // 原始命令串（日志/重组用）
    std::vector<std::string> args;    // 空白切分后的 argv
};

// 载入 roster 文件；false = 文件不可读（空文件合法 = 空表）
[[nodiscard]] bool load_roster(const std::string& path, std::vector<RosterEntry>& out);

struct SupervisorEvent {
    enum class Kind {
        Born,      // 拉起成功
        Died,      // 子进程退出（含退出码）
        Restarted, // 退避后重启
        GivenUp,   // 重启超限放弃
    };
    Kind kind;
    std::string name;
    int exit_code = 0;        // Died：waitpid 状态转出的退出码
    int restart_count = 0;    // 已重启次数（Restarted/GivenUp 语义）
};

class Supervisor {
public:
    // roster：花名册（拷贝入监督表，启动期 spawn_all 全量拉起）；
    // restart_limit：单条目重启上限（超限放弃）；backoff_base_ms：线性退避
    // 基数（第 n 次重启延迟 = n × backoff_base_ms）
    explicit Supervisor(const std::vector<RosterEntry>& roster, int restart_limit = 5,
                        std::uint32_t backoff_base_ms = 1000);

    // 拉起全部花名册条目（append 事件到 out）。已有存活子进程时幂等跳过
    void spawn_all(std::uint64_t now_ms, std::vector<SupervisorEvent>& out);

    // 主循环收割：非阻塞轮询全部子进程；死亡即按策略重启（退避期不重启，
    // 到点自动重生）。调用方以 ~50ms 节拍喂入（与主循环同源）
    void poll(std::uint64_t now_ms, std::vector<SupervisorEvent>& out);

    // 关停面：向全部存活子进程发 SIGTERM（不等待——骨架口径）
    void terminate_all() noexcept;

    [[nodiscard]] std::size_t alive_count() const noexcept;

private:
    struct Child {
        RosterEntry entry;
        bool alive = false;
        int pid = -1;
        int restart_count = 0;
        std::uint64_t respawn_at_ms = 0;  // 退避到期时刻（0 = 未排程重试）
    };

    void spawn_one(Child& c, std::uint64_t now_ms,
                   std::vector<SupervisorEvent>& out);

    std::vector<Child> children_;
    int restart_limit_;
    std::uint32_t backoff_base_ms_;
};

} // namespace machined
