#include "supervisor.hpp"

#include "apollo/core/log/log_manager.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <sstream>
#include <utility>

#if !defined(_WIN32)

#include <unistd.h>
#include <sys/wait.h>

namespace machined {

namespace {

// 库面日志出口：走默认 logger（machined 主程已接文件面；测试面落 console）
apollo::core::log::Logger& mlog() {
    return *apollo::core::log::global_log_manager().getDefaultLogger();
}

// 空白切分（骨架口径：不支持引号转义）
std::vector<std::string> split_args(const std::string& cmd) {
    std::vector<std::string> out;
    std::istringstream iss(cmd);
    std::string tok;
    while (iss >> tok) {
        out.push_back(tok);
    }
    return out;
}

// 两侧 trim（roster 列分隔值共用）
std::string trim_copy(const std::string& s) {
    const auto begin = s.find_first_not_of(" \t");
    if (begin == std::string::npos) {
        return {};
    }
    const auto end = s.find_last_not_of(" \t");
    return s.substr(begin, end - begin + 1);
}

// 纯数字判据（zone 列识别用）：非空且全为十进制数字。zone 列可省且命令
// 面可能含 '|'（shell 管道），故第三字段非纯数字时原样归命令（两列旧格式
// 兼容，见 supervisor.hpp roster 口径注）。
bool is_all_digits(const std::string& s) {
    if (s.empty()) {
        return false;
    }
    for (const char c : s) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    return true;
}

} // namespace

bool load_roster(const std::string& path, std::vector<RosterEntry>& out) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr) {
        return false;
    }
    char line[4096];
    while (std::fgets(line, sizeof(line), f) != nullptr) {
        std::string s(line);
        // 去尾换行
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) {
            s.pop_back();
        }
        const auto hash = s.find('#');
        if (hash != std::string::npos) {
            s = s.substr(0, hash);  // 行内注释截断
        }
        const auto bar = s.find('|');
        if (bar == std::string::npos) {
            continue;  // 无 name|command 分隔的行跳过
        }
        RosterEntry e;
        e.name = trim_copy(s.substr(0, bar));
        if (e.name.empty()) {
            continue;  // 空名跳过
        }
        e.command = s.substr(bar + 1);
        // 可选第二列：component_id（G-1 死亡上报身份；缺省/非法 = 0 未入编队）
        const auto cmd_bar = e.command.find('|');
        if (cmd_bar != std::string::npos) {
            const auto comp = trim_copy(e.command.substr(0, cmd_bar));
            e.command = e.command.substr(cmd_bar + 1);
            const unsigned long long parsed = std::strtoull(comp.c_str(), nullptr, 10);
            e.component_id = parsed;  // 非数字串 strtoull 得 0 = 未入编队

            // 可选第三列：zone_id（§6 死亡行窗口处置身份；纯数字判据——
            // 非纯数字 = 命令本身起始，原样保留两列旧格式兼容）
            const auto zone_bar = e.command.find('|');
            if (zone_bar != std::string::npos) {
                const auto zone_field = trim_copy(e.command.substr(0, zone_bar));
                if (is_all_digits(zone_field)) {
                    e.zone_id =
                        static_cast<std::uint32_t>(std::strtoul(zone_field.c_str(), nullptr, 10));
                    e.command = e.command.substr(zone_bar + 1);
                }
            }
        }
        const auto cmd_begin = e.command.find_first_not_of(" \t");
        if (cmd_begin == std::string::npos) {
            continue;  // 空命令跳过
        }
        e.command = e.command.substr(cmd_begin);
        e.args = split_args(e.command);
        if (e.args.empty()) {
            continue;
        }
        out.push_back(std::move(e));
    }
    std::fclose(f);
    return true;
}

Supervisor::Supervisor(const std::vector<RosterEntry>& roster, int restart_limit,
                       std::uint32_t backoff_base_ms)
    : restart_limit_(restart_limit)
    , backoff_base_ms_(backoff_base_ms) {
    children_.reserve(roster.size());
    for (const auto& e : roster) {
        Child c;
        c.entry = e;
        children_.push_back(std::move(c));
    }
}

void Supervisor::spawn_one(Child& c, std::uint64_t now_ms,
                           std::vector<SupervisorEvent>& out) {
    const pid_t pid = fork();
    if (pid < 0) {
        mlog().error(std::string("fork failed for ") + c.entry.name + ": "
                     + std::strerror(errno));
        // 拉起失败不进事件流，按一个退避单位延迟重试（alive 仍
        // false 且 respawn_at 已过——poll 重生路兜住）
        if (c.respawn_at_ms == 0) {
            c.respawn_at_ms = now_ms + backoff_base_ms_;
        }
        return;
    }
    if (pid == 0) {
        // 子进程：execvp 失败只能退出（127 惯例），由监督面按死亡处置
        std::vector<char*> argv;
        argv.reserve(c.entry.args.size() + 1);
        for (auto& a : c.entry.args) {
            argv.push_back(const_cast<char*>(a.c_str()));
        }
        argv.push_back(nullptr);
        execvp(argv[0], argv.data());
        // fork 后 pre-exec 路径保持直写 stderr——logger 面带锁，fork 快照
        // 可能停在他人持锁窗口，子进程触碰即死锁
        std::cerr << "[machined-child] exec failed: " << c.entry.command
                  << ": " << std::strerror(errno) << std::endl;
        std::_Exit(127);
    }
    c.alive = true;
    c.pid = pid;
    SupervisorEvent e;
    e.kind = SupervisorEvent::Kind::Born;
    e.name = c.entry.name;
    e.component_id = c.entry.component_id;
    e.zone_id = c.entry.zone_id;
    out.push_back(std::move(e));
}

void Supervisor::spawn_all(std::uint64_t now_ms,
                           std::vector<SupervisorEvent>& out) {
    for (auto& c : children_) {
        if (!c.alive) {
            spawn_one(c, now_ms, out);
        }
    }
}

void Supervisor::poll(std::uint64_t now_ms, std::vector<SupervisorEvent>& out) {
    // 收割：非阻塞轮询任一子进程
    for (;;) {
        int status = 0;
        const pid_t pid = waitpid(-1, &status, WNOHANG);
        if (pid <= 0) {
            break;  // 无死亡（0）或被信号打断（-1，EINTR 下一轮再来）
        }
        auto it = std::find_if(children_.begin(), children_.end(),
                               [pid](const Child& c) { return c.pid == pid; });
        if (it == children_.end()) {
            continue;  // 非花名册子进程（理论不达）——已收割即可
        }
        it->alive = false;
        it->pid = -1;
        SupervisorEvent e;
        e.kind = SupervisorEvent::Kind::Died;
        e.name = it->entry.name;
        e.component_id = it->entry.component_id;
        e.zone_id = it->entry.zone_id;
        // 退出码：正常退出取 WEXITSTATUS；信号终止取 128+signo（shell 惯例
        // ——死亡上报 wire 面为 uint32，负值无出口）
        e.exit_code = WIFEXITED(status) ? WEXITSTATUS(status)
                                        : (WIFSIGNALED(status) ? 128 + WTERMSIG(status) : -1);
        out.push_back(e);

        // 重启裁决：未超限 → 线性退避到点重生；超限 → 放弃
        if (it->restart_count < restart_limit_) {
            ++it->restart_count;
            it->respawn_at_ms =
                now_ms + static_cast<std::uint64_t>(it->restart_count) * backoff_base_ms_;
        } else {
            SupervisorEvent g;
            g.kind = SupervisorEvent::Kind::GivenUp;
            g.name = it->entry.name;
            g.component_id = it->entry.component_id;
            g.zone_id = it->entry.zone_id;
            g.restart_count = it->restart_count;
            out.push_back(std::move(g));
        }
    }

    // 退避到期重生（rc>0 = 死亡重启路，发 Restarted 事件；rc=0 =
    // fork 失败重试路，仅 spawn_one 的 Born 事件）
    for (auto& c : children_) {
        if (!c.alive && c.respawn_at_ms != 0 && now_ms >= c.respawn_at_ms) {
            c.respawn_at_ms = 0;
            const int rc = c.restart_count;
            spawn_one(c, now_ms, out);
            if (rc > 0) {
                SupervisorEvent r;
                r.kind = SupervisorEvent::Kind::Restarted;
                r.name = c.entry.name;
                r.component_id = c.entry.component_id;
                r.zone_id = c.entry.zone_id;
                r.restart_count = rc;
                out.push_back(std::move(r));
            }
        }
    }
}

void Supervisor::terminate_all() noexcept {
    for (auto& c : children_) {
        if (c.alive && c.pid > 0) {
            kill(c.pid, SIGTERM);
        }
    }
}

std::size_t Supervisor::alive_count() const noexcept {
    std::size_t n = 0;
    for (const auto& c : children_) {
        if (c.alive) {
            ++n;
        }
    }
    return n;
}

} // namespace machined

#else // _WIN32

namespace machined {

bool load_roster(const std::string& path, std::vector<RosterEntry>& out) {
    (void)path;
    (void)out;
    apollo::core::log::global_log_manager()
        .getDefaultLogger()
        ->error("supervisor not supported on this platform; roster ignored");
    return true;  // 空表语义：发现面照常
}

Supervisor::Supervisor(const std::vector<RosterEntry>&, int, std::uint32_t) {}
void Supervisor::spawn_all(std::uint64_t, std::vector<SupervisorEvent>&) {}
void Supervisor::poll(std::uint64_t, std::vector<SupervisorEvent>&) {}
void Supervisor::terminate_all() noexcept {}
std::size_t Supervisor::alive_count() const noexcept { return 0; }

} // namespace machined

#endif
