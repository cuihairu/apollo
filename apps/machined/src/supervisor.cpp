#include "supervisor.hpp"

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
        e.name = s.substr(0, bar);
        // name 两侧 trim
        const auto name_begin = e.name.find_first_not_of(" \t");
        if (name_begin == std::string::npos) {
            continue;  // 空名跳过
        }
        const auto name_end = e.name.find_last_not_of(" \t");
        e.name = e.name.substr(name_begin, name_end - name_begin + 1);
        e.command = s.substr(bar + 1);
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

void Supervisor::spawn_one(Child& c, std::vector<SupervisorEvent>& out) {
    const pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "[machined] fork failed for " << c.entry.name << ": "
                  << std::strerror(errno) << std::endl;
        return;  // 拉起失败不进事件流（下一 poll 会重试——alive 仍 false 且
                 // respawn_at 已过）
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
        std::cerr << "[machined-child] exec failed: " << c.entry.command
                  << ": " << std::strerror(errno) << std::endl;
        std::_Exit(127);
    }
    c.alive = true;
    c.pid = pid;
    SupervisorEvent e;
    e.kind = SupervisorEvent::Kind::Born;
    e.name = c.entry.name;
    out.push_back(std::move(e));
}

void Supervisor::spawn_all(std::vector<SupervisorEvent>& out) {
    for (auto& c : children_) {
        if (!c.alive) {
            spawn_one(c, out);
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
        e.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
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
            g.restart_count = it->restart_count;
            out.push_back(std::move(g));
        }
    }

    // 退避到期重生
    for (auto& c : children_) {
        if (!c.alive && c.restart_count > 0 && c.restart_count <= restart_limit_ &&
            c.respawn_at_ms != 0 && now_ms >= c.respawn_at_ms) {
            c.respawn_at_ms = 0;
            spawn_one(c, out);
            SupervisorEvent r;
            r.kind = SupervisorEvent::Kind::Restarted;
            r.name = c.entry.name;
            r.restart_count = c.restart_count;
            out.push_back(std::move(r));
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
    std::cerr << "[machined] supervisor not supported on this platform; "
                 "roster ignored" << std::endl;
    return true;  // 空表语义：发现面照常
}

Supervisor::Supervisor(const std::vector<RosterEntry>&, int, std::uint32_t) {}
void Supervisor::spawn_all(std::vector<SupervisorEvent>&) {}
void Supervisor::poll(std::uint64_t, std::vector<SupervisorEvent>&) {}
void Supervisor::terminate_all() noexcept {}
std::size_t Supervisor::alive_count() const noexcept { return 0; }

} // namespace machined

#endif
