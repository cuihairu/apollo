#pragma once
//
// 崩溃采集初始化（crash-capture 设计批②；docs/design/crash-capture.md §2）。
//
// 定位：进程级崩溃采集面——minidump 落盘（Crashpad out-of-process handler）。
// 初始化点 = 各 app main() 最早段（server.start() 前），采集面必须先立：
// 崩溃可能发生在其后任何段（日志/配置/服务器初始化）。
//
// 语义三条：
//   1. helper 自扫 argv（--crash-* 参数），app 参数循环零改动；
//   2. 失败降级不阻断（返回 false + warn，进程照常起）——采集面故障不成
//      服务面故障（fail-open）；
//   3. 编译期无 vcpkg/crashpad 环境 → no-op 桩（APOLLO_HAS_CRASHPAD=0），
//      三树照常绿（采集面是增强不是依赖）。
//
// 默认关、数据外发纪律：handler 不配 URL = 本地模式，报告滞留 Pending
// 不外发（logging 三禁 / net-abstraction §5.10 出站白名单同族）。

#include <string>

namespace apollo::runtime {

/// 初始化结果（供 app 侧日志/断言参考；失败不阻断）
struct CrashCaptureStatus {
    bool enabled = false;       ///< handler 是否成功启动
    bool compiled_in = false;   ///< 是否带 crashpad（APOLLO_HAS_CRASHPAD）
    std::string database_dir;   ///< dump 数据库目录（启用时有效）
    std::string handler_path;   ///< 定位到的 handler 可执行文件路径
    std::size_t pending_reports = 0; ///< 上次运行遗留的 Pending 报告数（sweep）
};

/// 初始化崩溃采集。
///
/// @param argc/argv  app 原始参数（helper 自扫 --crash-dump-dir /
///                   --crash-handler-path；其余参数原样透传给 handler）
/// @param proc_name  进程标识（缺省 db 目录 crashdumps/<proc_name>/ 与
///                   handler --annotation proc= 用）
/// @return 状态；enabled=false 时进程照常继续（fail-open）
///
/// 缺省:
///   db      = crashdumps/<proc_name>/（cwd 相对；编队各进程子目录自然隔离）
///   handler = 可执行文件旁 crashpad_handler（三级定位：显式 → 同级 → 编译期
///             缓存路径）
CrashCaptureStatus init_crash_capture(int argc, char* const argv[], const std::string& proc_name);

/// 触发一次故意崩溃（验收面 --crash-test 开关；smoke driver 同
/// --demo-anchors 先例——真实用途验证后即弃用注记）。
///
/// @param kind "null"=空指针解引用（验收链 §3 步骤 2）；未知 kind → false
/// @return 未知 kind 返回 false；已知 kind 不返回（进程崩溃）
bool crash_test(const std::string& kind);

} // namespace apollo::runtime
