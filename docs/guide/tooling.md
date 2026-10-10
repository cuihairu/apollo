---
title: 工具脚本
icon: terminal
order: 7
---

# 工具脚本（scripts/）

仓库自带的运维/验收脚本一览。设计依据与交付注记见各归属设计件（崩溃采集 / 备份容灾 / 容量基准）。

## dev 编队：dev_fleet.sh

包装 machined 监督面（P3-4 批 A）的一键起停。编队 = 生命周期半边（拉起/重启/收割），不含目录注册语义。

```bash
scripts/dev_fleet.sh up      # 生成 roster 并后台拉起 machined（含编队子进程）
scripts/dev_fleet.sh status  # machined 存活 + 子进程清单
scripts/dev_fleet.sh down    # SIGTERM machined（批 F 关停面收割 + 超时强杀 + 孤儿兜底）
scripts/dev_fleet.sh roster  # 打印本次编队 roster（未构建的 app 标记跳过）
```

- 构建树须 `GAME_MODULE=ON` 且全 app 已构建（`APOLLO_BUILD_DIR` 可调，默认 `<repo>/build`）。
- 运行目录 `APOLLO_FLEET_DIR`（默认 `/tmp/apollo-fleet-<uid>`：roster + pidfile + 日志）。
- dev 端口布局（脚本约定，无配置真相）：login 9001 / gateway 9002 / manager 9003 / zone-app 9004 / cell 9005 / game（bootstrap 演示退出）/ logger（follow 聚合面，无端口——ADR-013 L3：各 app 结构化文件 log/*.log 的编队汇聚行进 fleet.log，`logger scan` 可离线检索同目录）。
- 已知边界：ingress 未接线（`acceptNewConnections` 仍是保活空循环），gateway 起得来但无客户端入口——「退出码」边界已消账。**处方状态（2026-10-10）**：三步全落地——②roster 补传三后端 URL（5ccfc651，gateway 连接参数显式化）、①start 连接失败降级警告+空件返回/③ChatApp 幽灵依赖四处摘除（d7f21831）；无后端 standalone 冒烟实证三后端降级警告全出、存活至超时（原 `exit=1` 不复现）；gateway 仍为无入口壳，ingress 接线挂 net M1 拍板链（根因三因与处方见 todo P3-4 批记）。
- 实机走查（2026-10-10）：up → status → down 全链复验——born×6、game bootstrap 演示 exit=0、gateway exit=1 退避×5 give-up，与批 A 冒烟口径一致；**走查修一处缺陷**：down 的孤儿兜底原为 `pgrep -P <死 pid>`（machined 死后子进程重挂 init，恒空=死代码），login-app 慢关（~3-5s）会在「编队已停」后暂留孤儿——改为按 roster 二进制名 `pgrep -x`/`pkill -x` 精确清理（共享 5s 宽限后 KILL 残留），复验 down 返回即零孤儿。

## kill -9 全链演练：drill_kill9.sh

G-2 备份容灾批 0 验收链（backup-revive §6）：编队内 kill -9 zone-app，实测「死亡上 wire → 退避重启 → journal replay → 恢复相位 ready → 全量重报收敛 → 目录镜像收敛」各段 RTO。

```bash
scripts/drill_kill9.sh       # 全自动：起编队 → kill -9 → 断言收敛 → 打印 RTO 表 → 收割
```

- 独立端口段（19003/19004/19600-19602），可与 dev_fleet 共存不冲突。
- 运行目录 `APOLLO_DRILL_DIR`（默认 `/tmp/apollo-drill-kill9-<pid>`，fleet.log 留档）。
- 会话面为 `--demo-anchors 3` 冒烟种子；journal replay 段为空档下界口径，客户端 resume 在目录级。

## 崩溃符号分离：split_symbols.sh

部署面符号管理（crash-capture §2.3）。每个二进制三步：`objcopy --only-keep-debug` 产 `.debug` → `strip --strip-debug` 产发行件 → `objcopy --add-gnu-debuglink` 以 build-id 同源关联。

```bash
scripts/split_symbols.sh <binary> [<binary> ...]
```

- Linux-only（部署目标口径），binutils 缺失显式报缺。
- `.debug` 收进运维符号仓；还原管线（下节工具）按 build-id 回填符号化。

## 崩溃处理工具按需构建：build_crash_tools.sh

构建 Breakpad 工具面（`dump_syms` + `minidump_stackwalk`）——客户端采集用 Crashpad、处理面用 Breakpad 工具（minidump 同族组合），只取 tools、零 Breakpad 客户端引入。

```bash
scripts/build_crash_tools.sh                 # shallow clone breakpad + lss → configure → make
tools/crash-bin/dump_syms  <bin> > x.sym     # ELF+DWARF → 符号文件
tools/crash-bin/minidump_stackwalk --symbol-path <symdir> <dump>   # minidump → 符号化栈
```

- 产物收 `tools/crash-bin/`（.gitignore）；`BREAKPAD_SRC` 可调源码/构建目录。
- **不入三树门禁**：验收/运维一次件，断网/构建失败显式报错并非零退出，不静默不回退。
- 符号构建须 `APOLLO_ENABLE_DEBUG_SYMBOLS=ON`（Release 附加 `-g`，GCC 侧钉 `-gdwarf-4`——DWARF5 下 dump_syms 静默丢函数/行号，crash-capture §2.3 批记）。

## CI 辅助（scripts/ci/）

- `contract_pack.sh`：契约打包（SDK 分发面）。
- `schema_hash_report.sh`：契约 schema 哈希报告（client_hash 双域纪律核对）。

## 相关文档

- [崩溃采集](/design/crash-capture) —— 符号面与验收链全链
- [备份容灾与宕机接管](/design/backup-revive) —— 批 0 演练链与 RTO 数字
- [容量与基准](/design/capacity-and-benchmark) —— 指标口径
- 测试套件：仓库 `tests/README.md` —— 门禁口径与新增测试惯例
