#pragma once

// apolloc——项目初始化 CLI 的唯一入口（P0.2 L36 入口，L37 模板）。init
// 子命令在目标目录生成最小项目模板：配置、Lua 入口、契约入口、目录说明、
// 忽略规则与启停脚本。模板内容由 templates.hpp 集中提供；本层只做校验、
// 落盘与分派。不向每个服务复制初始化器：全仓只有这一个 init 实现，服务
// 二进制只管自己的运行参数。
//
// 退出码对齐 fleetctl 三态口径：
//   0 = 成功/help；1 = 用法错误（无参/未知子命令/init 缺或多参数）；
//   2 = 目标拒绝（非空目录/已存在文件/空 path/目录穿越/非法项目名/文件系统失败）。
// 目标已存在非空即拒收，绝不覆盖已有内容；目录穿越路径（含 .. 分量）在
// 落盘前拒绝。文件一律 binary + LF（Windows 不做 CRLF 翻译，生成物跨平台
// 字节一致）。

#include "apollo_cli/templates.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace apollo_cli {

inline void print_usage(std::ostream& out) {
    out << "apolloc - project scaffold CLI\n"
        << "\n"
        << "Usage:\n"
        << "  apolloc init <path>   create a minimal project skeleton at <path>\n"
        << "  apolloc help          show this help\n"
        << "\n"
        << "init creates the minimal project template: configs, Lua entry,\n"
        << "contract entry, directory guide, ignore rules and start/stop\n"
        << "scripts. Pending pieces stay marked in the generated README.\n"
        << "CI belongs to the repository workflow, not to this tool.\n";
}

// 生成模板：五个目录占位（run/ 是逐角色运行根，L38）+ templates.hpp 清单
// 里的全部文件。成功 true；失败 false 且 err 已填原因。调用方保证 target
// 已通过校验。
inline bool scaffold_project(const std::filesystem::path& target,
                             const std::string& project_name,
                             std::string& err) {
    std::error_code ec;
    for (const char* dir : {"config", "scripts", "lua", "contract", "run"}) {
        std::filesystem::create_directories(target / dir, ec);
        if (ec) {
            err = "cannot create directory '" + (target / dir).string() +
                  "': " + ec.message();
            return false;
        }
    }

    // 清单驱动落盘：模板内容与路径映射集中 templates.hpp，本层只管文件 IO。
    for (const auto& file : scaffold_files(project_name)) {
        const std::filesystem::path path = target / file.first;
        std::filesystem::create_directories(path.parent_path(), ec);
        if (ec) {
            err = "cannot create directory '" + path.parent_path().string() +
                  "': " + ec.message();
            return false;
        }
        std::ofstream out(path, std::ios::binary);
        out << file.second;
        if (!out) {
            err = "cannot write '" + file.first + "' under '" + target.string() +
                  "'";
            return false;
        }
    }
    return true;
}

// 项目名落进生成的 CMake project() 与文档首行，必须是安全标识符：字母或
// 下划线开头，后续允许数字、下划线、连字符与点。空格会让 CMake 无法
// configure，分号/引号可注入命令或字符串，数字/点开头不是合法标识符。
// 不安全即拒收（退出 2），绝不生成坏文件。
inline bool is_safe_project_name(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    const auto head_ok = [](char c) {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_';
    };
    const auto body_ok = [&head_ok](char c) {
        return head_ok(c) || (c >= '0' && c <= '9') || c == '-' || c == '.';
    };
    if (!head_ok(name.front())) {
        return false;
    }
    for (const char c : name) {
        if (!body_ok(c)) {
            return false;
        }
    }
    return true;
}

// 校验并受理 init 目标；通过时调用 scaffold_project。失败 false 且 err 已填。
inline bool init_project(const std::string& raw_target,
                         std::ostream& out, std::string& err) {
    if (raw_target.empty()) {
        err = "init: <path> must not be empty";
        return false;
    }
    // 穿越检查在归一化之前做：lexically_normal 会把 "a/../b" 折叠成 "b"，
    // 先查原始分量才能拦下越界意图。
    for (const auto& part : std::filesystem::path(raw_target)) {
        if (part == "..") {
            err = "init: path must not contain '..' components: '" + raw_target + "'";
            return false;
        }
    }
    const std::filesystem::path target =
        std::filesystem::path(raw_target).lexically_normal();

    std::error_code ec;
    const bool exists = std::filesystem::exists(target, ec);
    if (ec) {
        err = "init: cannot inspect '" + target.string() + "': " + ec.message();
        return false;
    }
    if (exists) {
        if (!std::filesystem::is_directory(target)) {
            err = "init: '" + raw_target + "' exists and is not a directory";
            return false;
        }
        const bool empty = std::filesystem::is_empty(target, ec);
        if (ec) {
            err = "init: cannot inspect '" + target.string() + "': " + ec.message();
            return false;
        }
        if (!empty) {
            err = "init: '" + raw_target + "' is not empty; refusing to overwrite";
            return false;
        }
    }

    const std::string project_name = target.filename().string();
    if (!is_safe_project_name(project_name)) {
        err = "init: project name '" + project_name +
              "' is not safe (use letters/digits/'_'/'-'/'.' and start with a "
              "letter or '_')";
        return false;
    }
    if (!scaffold_project(target, project_name, err)) {
        return false;
    }
    out << "created project skeleton at " << target.string() << "\n";
    return true;
}

// 子命令分派（纯函数直测面；main 只做薄壳）。返回值即进程退出码。
inline int run(int argc, char* argv[], std::ostream& out, std::ostream& err) {
    if (argc < 2) {
        print_usage(err);
        return 1;
    }
    const std::string command = argv[1];
    if (command == "help" || command == "--help" || command == "-h") {
        print_usage(out);
        return 0;
    }
    if (command == "init") {
        if (argc == 2 || argc > 3) {
            err << "init: expected exactly one <path> argument\n";
            print_usage(err);
            return 1;
        }
        std::string reason;
        if (!init_project(argv[2], out, reason)) {
            err << reason << "\n";
            return 2;
        }
        return 0;
    }
    err << "unknown command '" << command << "'\n";
    print_usage(err);
    return 1;
}

} // namespace apollo_cli
