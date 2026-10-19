// apolloc——项目脚手架 CLI 壳入口（P0.2 L36）：init 子命令的唯一宿主。
// 分派逻辑在 apollo_cli/cli.hpp（header-only，测试直编同源）；本文件只做
// 薄壳。不接崩溃采集：一次性落盘工具（非长驻进程），crash banner 会污染
// stdout 查询面且在调用者 cwd 落 crashdumps/，失败信息走 stderr + 退出码
// 已可定位，与 admin 显式关 console appender 同口径。

#include "apollo_cli/cli.hpp"

#include <iostream>

int main(int argc, char* argv[]) {
    return apollo_cli::run(argc, argv, std::cout, std::cerr);
}
