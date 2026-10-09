# 崩溃采集设计（crash-capture）

> 状态：**已交付（批②③④ 2026-10-09 网络恢复续批；批①设计件 2026-10-08）**。**选型已钉（2026-10-08 用户拍板）：Google Crashpad，C++ 直集成**——Breakpad 只作历史对照，不引入其客户端。定位：进程级崩溃采集面——minidump 落盘、符号化还原、上传留位（默认关）。缺口登记：本件即崩溃采集批设计面（todo P3-3 批记）。互引：logging.md（观测三禁/外发白名单纪律）、net-abstraction §5.10（出站目标白名单——上传面同纪律）、capacity-and-benchmark §5（基准设施之外的第二类「运维证据」面）、improvement-plan P3-3（可观测性：崩溃报告=事后观测面）。

---

## 执行摘要

1. **为什么 Crashpad（Breakpad 历史对照）**：核心差异在**采集进程模型**——Crashpad 用**独立 handler 进程**（out-of-process）：目标进程崩溃瞬间由 crashpad 注入的信号处理只做最小事（搬运寄存器/线程上下文），minidump 全部由活着的 handler 进程写盘；Breakpad 是 in-process 信号处理器里直接写 minidump——堆损坏/栈耗尽/信号处理器内再崩溃（二次故障）场景下自采面不可靠，这正是 Breakpad→Crashpad 的演进动机。配套面：Crashpad 带 CrashReportDatabase（报告状态机 New/Pending/Completed）与 Settings（上传开关持久化），Breakpad 无对应物；维护面：Breakpad 客户端最后 release 2022-02、Crashpad 活跃（Chrome 主力采集器）。**工具链复用 Breakpad 侧是既定事实**：minidump 格式同族（Microsoft MDMP），Crashpad 自身不带 CLI stackwalker，dump_syms/minidump_stackwalk（Breakpad tools）是事实标准处理端（Sentry 同款组合：crashpad 客户端 + breakpad 处理器）——本设计工具面走同一路，客户端面零 Breakpad。
2. **接入走 vcpkg manifest**（合仓内 vcpkg 纪律，不引 FetchContent/子模块第三条路）：**overlay 端口 2026-07-02 + override**——仓内 baseline `8f7641a3` 钉 crashpad 2024-04-11#9，对本机 GCC 15.2 缺适配；2026-07-02 端口含 GNU 处理（`mini_chromium_is_clan=false` 修正为 `mini_chromium_is_clang=false` + `extra_cflags="-Wno-error"`）与 mini_chromium toolchain patch。**本机编译实证待网络恢复补验**（2026-10-08 外网中断：vcpkg-tool-gn 二进制下载源 chrome-infra-packages.appspot.com TLS 全断——端口文件、GN 构建托管与产物清单均按本地 vcpkg 端口树实读核过，编译验证随批②接线一并补）。overlay 端口 vendored 进仓（`ports/vcpkg-overlays/crashpad/`，6 文件：portfile.cmake / vcpkg.json / crashpadConfig.cmake.in / fix-missing-stdint.patch / mini-chromium-toolchain.patch / zlib.gn），GN 构建在 vcpkg 工具内闭环（vcpkg-tool-gn 托管），CMake 侧只见 `find_package(crashpad CONFIG)` → `crashpad::crashpad` 接口目标——**不引 depot_tools、不碰 GN**。
3. **初始化点 = 各 app main() 最早段**（参数解析后、server.start() 前）调 `runtime::init_crash_capture(argc, argv, proc_name)`——崩溃可能发生在其后任何段（含日志/配置初始化），采集面必须先立。helper 自扫 `--crash-*` 参数（app 参数循环零改动）；init 失败**降级不阻断**（返回 false + warn 日志，进程照常起——采集面故障不能成为服务面故障，fail-open）。
4. **dump 落盘**：`crashdumps/<proc>/`（cwd 相对缺省，编队各进程子目录自然隔离——machined roster 拉起子进程继承 cwd，fleet 目录下即 `crashdumps/base-app/`）；`--crash-dump-dir` 覆盖。数据库形态 = Crashpad SimpleStringDatabase（db 目录内 settings.dat + reports/）。
5. **handler 打包**：构建期 `find_file` 定位 vcpkg tools 产物 `crashpad_handler`，POST_BUILD 复制到各 app 可执行文件旁（**产物带 handler**）；运行期 `--crash-handler-path` 覆盖。部署面无需额外步骤——handler 与 app 同目录分发。
6. **符号表管理**：`APOLLO_ENABLE_DEBUG_SYMBOLS` 选项（Release 附加 `-g`，默认关——符号面不影响行为，三树体积/链接时长不动）；部署分离 `scripts/split_symbols.sh`（objcopy `--only-keep-debug` 产 `.debug` + `strip --strip-debug` 产发行件 + build-id 关联，.debug 按需回填）；还原管线 = `dump_syms`（ELF+DWARF → .sym）→ `minidump_stackwalk --symbol-path`（minidump → 符号化栈）。工具面按需构建（`scripts/build_crash_tools.sh` clone google/breakpad + configure+make，产物进 `tools/crash-bin/`）——**不入三树门禁**（验收/运维一次件，门禁时间预算纪律）。
7. **上传留位（默认关）**：handler 初始化**不配 URL**——本地模式，报告滞留 Pending 不外发。上传启用属**数据外发**（logging 三禁/出站白名单同族纪律），默认关；未来打开 = handler `--url` + `Settings::SetUploadsEnabled(true)` 两点，接口留位不实现、不引上报服务。
8. **与既有日志/监控打通**：init 即日志一行（enable/db 路径/handler 路径/上传关）；启动期扫库 pending 计数 → 日志 + MetricRegistry 计数（事后观测面——崩溃进程已死，活进程只能看见上一次运行的遗留，sweep 即「遗留可见化」）；machined 监督面死亡日志（exit 128+signo 惯例，批 F 已有）与 dump 落盘互为印证，不做联动阻断。

## 1. 选型对照与 vcpkg 现状

### 1.1 Breakpad 历史对照（选型已钉，本节只记理由存档）

| 维度 | Crashpad（选用） | Breakpad（对照） |
|---|---|---|
| 采集模型 | 独立 handler 进程——目标进程崩溃后只做上下文搬运，dump 由活进程写 | in-process 信号处理器直接写 dump——二次故障（堆损坏/栈耗尽/处理器内再崩）面不可靠 |
| 报告管理 | CrashReportDatabase（New/Pending/Completed 状态机）+ Settings（上传开关持久化） | 无（文件即报告，磁盘自管） |
| 维护状态 | 活跃（Chrome 主力采集器，chromium.googlesource） | 客户端停维（最后 release 2022-02；工具链 dump_syms/minidump_stackwalk 仍是事实标准，继续用） |
| 依赖 | zlib（linux 另需 curl——发送端依赖，上传关不掉依赖只能显式链） | 自包含 |

### 1.2 vcpkg 现状与 overlay 决策

- baseline `8f7641a3`（vcpkg.json `builtin-baseline`）钉 crashpad **2024-04-11#9**——早于 GCC 15（2025-04 release），缺 GNU 适配，本机 GCC 15.2 下 GN 构建预期红；**不升 baseline**（升基线牵动 openssl/protobuf/gtest 等全部依赖版本重解析，爆炸半径不可控），走 **overlay 端口 + override**：仓内 `ports/vcpkg-overlays/crashpad/` vendored vcpkg 2026-07-27 工具版同名端口（version-date **2026-07-02**，上游 crashpad @ `efdc820b`），vcpkg.json 加 `"crashpad"` 依赖 + overrides 钉 2026-07-02 + vcpkg-configuration.json 声明 overlay-ports。overlay 五文件：portfile.cmake / vcpkg.json / crashpadConfig.cmake.in / fix-missing-stdint.patch / mini-chromium-toolchain.patch / zlib.gn。
- 传递依赖如实记：linux 平台 crashpad 端口依赖 **curl**（发送端链接面）——与 net-abstraction §5.10「curl 进 vcpkg」裁决同向，顺路兑现依赖面；vcpkg 端口同时拉 vcpkg-tool-gn/vcpkg-gn（GN 构建托管，host 依赖）。
- **产物清单（端口口径，本机装机实证随批②补）**：`${VCPKG_INSTALLED_DIR}/<triplet>/lib/{vcpkg_crashpad_client,vcpkg_crashpad_client_common,vcpkg_crashpad_util,vcpkg_crashpad_base}.a` + `include/crashpad/**` + tools 目录 `crashpad_handler` 可执行文件；CMake 消费面 `find_package(crashpad CONFIG REQUIRED)` → `crashpad::crashpad`（接口目标，含 ZLIB::ZLIB 传递链接）。
- 门禁影响：三树 configure 各加 crashpad 一次构建（binary cache 复用后增量近零）；find_package 失败（非 vcpkg/无端口环境）→ crash 采集 no-op 桩（`APOLLO_HAS_CRASHPAD=0`），三树照常绿——采集面是增强不是依赖。

## 2. 集成设计

### 2.1 初始化点与时序

```text
main(argc, argv)
  └─ runtime::init_crash_capture(argc, argv, "<proc>")     ← 最早段（server.start() 前）
       ├─ 自扫 argv：--crash-dump-dir / --crash-handler-path（app 参数循环零改动）
       ├─ 缺省：db=crashdumps/<proc>/（cwd 相对）、handler=可执行文件旁 crashpad_handler
       ├─ CrashpadClient::StartHandler(handler, db, metrics="", url="",     ← url 空=本地模式
       │                              arguments={}, database_config, ...)   ← attachments 留位
       ├─ 成功 → APOLLO_LOG info 一行 + APOLLO_HAS_CRASHPAD=1
       └─ 失败 → APOLLO_LOG warn 一行 + 返回 false——进程照常起（fail-open）
  └─ …（日志/配置/服务器初始化——此后任何段崩溃均有采集）
```

- **为什么最早**：采集面保护的正是「其后的全部段」——日志/配置/服务器初始化自身都可能是崩溃点；init_crash_capture 自身只依赖参数与文件系统，不依赖日志面（日志不可用时走 stderr 兜底一行）。
- **语义边界**：init 只在 `StartHandler` 成功即返回——handler 进程由 crashpad 自行 spawn/管理（app 不 fork 不 wait）；附件流（attachments）接口留位不实现。
- **平台口径**：部署目标 Linux（net-abstraction §5.8 服务器 Linux-only 同口径）；port supports 表达式含 win/osx，非 Linux 平台 find_package 成功即同 API 可用，失败即 no-op 桩——不写平台分支代码。

### 2.2 dump 目录与 handler 打包

- 目录：`crashdumps/<proc>/`（proc 名 helper 显式传入，与进程身份一致）；`--crash-dump-dir` 整目录覆盖（编队与单进程调试两用）。machined roster 拉起子进程继承 cwd（dev_fleet up 实测口径），fleet 目录下各 app 子目录自然隔离，无共享目录竞争。
- handler 定位三级：①`--crash-handler-path` 显式；②可执行文件目录同级 `crashpad_handler`（POST_BUILD 复制保证——产物自带）；③vcpkg tools 路径（构建期 find_file 缓存，编译进缺省值）。三级落空 → init 失败 → fail-open（采集关、服务照常）。
- 打包纪律：crashpad_handler 与 app 同目录分发即完整部署面；版本一致性靠同一次构建产出（port 版本钉死在 overlay——handler 与 client 库同源，无跨版本错配面）。

### 2.3 符号表管理

- **build 留 -g**：`APOLLO_ENABLE_DEBUG_SYMBOLS`（ON/OFF/Examples 三树共用选项，默认 OFF）。开启后 Release 追加 `-g -gdwarf-4`（选项定义后、target 创建前接线——add_compile_options 目录域只影响其后 target，初版误置 legacy 死块内实为 no-op，符号化实跑批修正；不触碰优化档）；不开启时 CMake 默认不 strip，ELF symtab 仍含函数名——dump_syms 产函数级符号（无行号），验收/线上两档可用。**-gdwarf-4 钉档（2026-10-09 实跑实证）**：GCC 15.2 缺省 DWARF5 下 breakpad dump_syms 对含 crashpad/重模板头树的 CU 静默丢 FUNC/LINE（542 vs 1010 FUNC 记录、模块面零行号），DWARF4 全量解析（1010 FUNC + 74674 LINE）——符号化构建档随钉，DWARF5 退化构造型待 breakpad 侧跟进再评估。
- **分离（部署面）**：`scripts/split_symbols.sh <binary>`——`objcopy --only-keep-debug` 产 `<binary>.debug`、`strip --strip-debug` 原地产发行件、`objcopy --add-gnu-debuglink` 关联（build-id 同源）；`.debug` 收进符号仓（运维面），发行件不含符号。
- **还原管线**：`dump_syms <binary> > symbols/<name>/<build-id>/<name>.sym`（Breakpad 目录约定）→ `minidump_stackwalk --symbol-path symbols/ <dump>` → 帧级符号化栈（函数/文件/行，取决于 -g 档）。
- **工具构建**：`scripts/build_crash_tools.sh`——shallow clone google/breakpad + `./configure && make`，产物 `dump_syms`/`minidump_stackwalk` 收 `tools/crash-bin/`（.gitignore）；**不入三树门禁**（验收/运维一次件；breakpad 客户端面零引入，只取 tools）。断网/构建失败 → 验收面显式报缺，不静默。

### 2.4 上传留位（默认关，数据外发纪律）

- handler 初始化 URL 空 = 本地模式：报告写库后滞留 **Pending**，Crashpad 不外发（Settings::SetUploadsEnabled 缺省 false 双保险）。
- 上传属数据外发：minidump 含进程内存快照（用户数据面）——logging.md 观测三禁与 net-abstraction §5.10 出站白名单同族纪律适用；打开 = handler `--url`（上报端点）+ `Settings::SetUploadsEnabled(true)`（两处，均在拍板/审批面）+ 白名单目标进配置。**本批不实现外发面**，接口留位（StartHandler 参数形已在位）。

### 2.5 与日志/监控打通

- **init 行**：crash 采集 enable/disable、db 目录、handler 路径、上传关——进程身份进入日志面（logging §5.1 结构化行格式，cat=crash）。
- **启动 pending sweep**：`CrashReportDatabase::Initialize(db)` → `GetPendingReports()` 计数 → 日志（cat=crash, pending=N）+ MetricRegistry 计数器（`crash_reports_pending`，P3-3 批 B 最小集消费面）——崩溃是事后事件，活进程的观测面=「上一次运行的遗留可见化」；sweep 只读不改报告状态（状态迁移归上传面/清理面，本批不动）。
- **machined 互证**：监督面死亡日志（Died 事件 exit=128+signo，批 F 口径）与 dump 落盘同一事件的两面——崩溃进程的 dump 在 `crashdumps/<proc>/`、死亡行在 machined 日志，运维对照表记入 §3 验收链。不做联动阻断（采集失败/失败不拉闸，fail-open 一致）。

## 3. 验收链（本批兑现，Linux 本机）

1. 带符号构建：ON 树 configure `-DAPOLLO_ENABLE_DEBUG_SYMBOLS=ON`（验收面专用，非门禁常开）。
2. 故意崩溃：`base-app --crash-test null`（空指针样例开关，smoke driver 面同 --demo-anchors 先例——真实用途验证后即弃用注记）→ SIGSEGV → crashpad_handler 写 dump 至 `crashdumps/base-app/`。
3. 工具就绪：`scripts/build_crash_tools.sh` → `tools/crash-bin/{dump_syms,minidump_stackwalk}`。
4. 符号化：`dump_syms base-app → .sym`（Build id 关联）→ `minidump_stackwalk --symbol-path` → 栈含 crash 测试函数与调用帧。
5. 遗留可见化：重启 base-app → init 行 + pending=N 日志 + MetricRegistry 计数。
6. machined 对照：dev_fleet 下同崩溃 → machined Died 行（exit=139=128+SIGSEGV）与 dump 落盘两面互证。

## 4. 分期

- **批①（2026-10-08）**：①设计件 + 本机 overlay vendoring 先行；②③④ 因外网中断（vcpkg-tool-gn 二进制下载不可达）暂停。
- **批②③④（2026-10-09，网络恢复续批，todo P3-3 批记详）**：②vcpkg 接线（manifest 依赖 + overrides + overlay-ports 声明；install 全量构建实证）+ runtime helper（`init_crash_capture`：argv 自扫 / 缺省目录 / handler 三级定位 / 本地模式 / fail-open 桩）+ 全 app 接线（七 app main 最早段）+ handler 打包（POST_BUILD 复制+chmod——vcpkg tools 产物无执行位实证补；crashpad_FOUND/导入 target 目录域两坑，函数判定走 find_file CACHE）；③符号面（APOLLO_ENABLE_DEBUG_SYMBOLS 选项 + split_symbols.sh + build_crash_tools.sh[不入门禁]）+ --crash-test null 验收开关 + 启动 pending sweep（日志 + MetricRegistry 计数）。**验收链实测**：init 行 → 故意崩溃 exit=139 → dump 落 pending/ → 重启 pending=1 全链通。**符号化实跑（同日续）**：build_crash_tools.sh 实构建通过（dump_syms/minidump_stackwalk 落 tools/crash-bin/）→ dump_syms 产 .sym → minidump_stackwalk 栈回溯 `crash_test`[crash_capture.cpp:163 精确到行]/`main`[main.cpp:73]——build-id 三方互证（ELF note ≡ minidump CV record ≡ sym CODE_ID）；顺手修正选项接线 no-op bug（死块内实不生效——实跑暴露，接线前移+gdwarf-4 钉档见 §2.3）。§3 验收链 1-5 全过；步骤 6（machined 对照）随编队冒烟批。
- **后续（不本批）**：上传面打开（数据外发审批 + 白名单端点，挂拍板）；dump 保留策略/清理面（Pending 滞留磁盘上限——量级小，先观察）；Windows CI 适配（port supports 可用，apollo Windows 仅开发机垫片，非目标）；attachments 附件流。

## 5. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| logging | init 行/pending 行走结构化行格式（cat=crash）；三禁纪律=上传默认关的依据之一 |
| net-abstraction §5.10 | 出站白名单纪律——上传面打开时的目标约束；curl 依赖同向顺路 |
| capacity-and-benchmark | 「每项先有证据」纪律的崩溃面同型：dump+符号栈=事后证据，与基准数据同一「运维证据」家族 |
| improvement-plan P3-3 | 可观测性批的崩溃面增量；MetricRegistry 计数器消费面（批 B 最小集） |
| architecture-review §15.2 | 依赖纪律：vcpkg manifest 单一依赖面（overlay 是 vcpkg 内机制不是第二依赖体系）；不引 depot_tools/GN 用户面 |
| scripts/dev_fleet.sh | 编队冒烟与崩溃采集的 cwd/日志目录共存面（crashdumps/ 与 fleet 日志并列） |

---

*基线：apollo main @ a63855ac。本机环境：GCC 15.2.0 / CMake 4.2.3 / vcpkg 工具 2026-07-27（编译实证 2026-10-09 随批②补验完成——crashpad:x64-linux@2026-07-02 全量构建通过，产物四静态库+crashpad_handler+头树）；crashpad overlay 端口 version-date 2026-07-02（上游 chromium.googlesource crashpad @ efdc820b，vcpkg 端口自 vcpkg 2026-07-27 工具版 vendored）。Breakpad 工具链按需构建（google/breakpad master），产物不入门禁。上传面/外发默认关——数据外发纪律（logging 三禁/出站白名单）同族，打开归拍板/审批面。*
