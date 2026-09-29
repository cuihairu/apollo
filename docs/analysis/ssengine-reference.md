# SSEngine 老引擎参考（ssengine-reference）

> 分析对象：`/home/cui/workspaces/SSEngine`——旧引擎二进制反推的 C++17 系统层（线程同步/定时器/内存池/对象池/文件 IO/共享内存/线程池/DB facade/日志/网络封装）。本仓库自带逆向分析产物（FINAL_ANALYSIS_REPORT.md、IMPLEMENTATION_ROADMAP.md、QUICK_REFERENCE.md 等）与 `include/ssengine/` + `src/` 全部源码。
> 立场：逐模块分析"对 apollo 有没有价值"，结论只分**值得引入（给落地形态）/ 已有且更优 / 不建议引入**。

---

## 执行摘要

1. **最值得引入：异步 DB Command 模型**（`sddb.h:253-390`）：`OnExecuteSql`（DB 线程执行）→ `OnExecuted`（主线程 `Session::Run()` 回读）→ `Release` 的两段式异步，加 `GetGroupId()` 保序语义——这是 MMO"DB 线程 + 主线程完成回调"的标准解，apollo `modules/data` 目前只有同步 mock，此模型是第一优先参考。
2. **第二：类型化对象池**（`sdobjectpool.h`）：placement-new + free-list + **锁策略作为模板参数**（`MT = CSDNonMutex`）；apollo 只有裸内存池（`utils/memory_pool.h`），缺按类型的池化。C++20 落地建议用 pmr/free-list 现代化重写，不搬宏。
3. **第三：真·定时器轮**。sdtimer.h 头注释宣称"Linux 风格分层定时器"，`sdtimer.cpp:21-79` 实际是 **mutex + vector 全表扫描**——文档与实现不符是本仓库第一教训；apollo 目前**没有定时器模块**，游戏循环需要的是真分层时间轮（O(1) 插入/删除），按"目标设计"吸收而非按"现有实现"。
4. **背压可见性值得学习**：sdnet 的 `GetSendBufFree()`（`sdnet.h:166-169`）与 `DelaySend`（跨线程投递）——把发送队列水位显式暴露给上层；apollo 网络抽象层（docs/design/net-abstraction.md）把这类能力收进统一接口，而不是像老引擎那样直接暴露线程与缓冲区细节。
5. **属性/状态同步：SSEngine 没有**。它是纯系统层库（线程/内存/文件/网络管道），不含任何实体属性复制机制——apollo 的属性同步必须以 BigWorld/KBEngine 类模型为目标（见 `docs/design/attribute-sync.md`），老引擎能借的只有传输层零件（sdpkg 定长帧头+校验、loop_buffer 环形缓冲、data_queue 线程安全队列）。
6. **教训清单**（反推过程暴露）：同一仓库两份分析结论互相矛盾（"100% 完成/生产就绪" vs "20 个核心模块无实现、覆盖率 59.7%"）；头文件注释撒谎（分层定时器）；`volatile BOOL` 做线程标志（`sdthreadpool.h:140-141`）；"remember to lock" 注释式线程安全（`sdmemorypool.h:29-41`）；裸指针 + `Release()` 手工生命周期（`sddb.h:138`）。**引思想、引设计，不引代码。**

---

## 1. 背景与阅读范围

- 来源：旧 MMO 引擎系统层的二进制反推重建，C++17，Windows/Linux 双栈（`win32/`、`win64/` 为预编译产物）。
- 已读：FINAL_ANALYSIS_REPORT.md、IMPLEMENTATION_ROADMAP.md、QUICK_REFERENCE.md、`include/ssengine/` 全部 70+ 头文件（重点：sdtimer/sdmemorypool/sdobjectpool/sdthreadpool/sdtime/sddb/sdidpool/sdshmem/sdpkg/sddatastream/sdnet）与关键 .cpp（sdtimer.cpp 等）。
- 可信度判断（重要）：仓库内三份分析材料自相矛盾——
  - FINAL_ANALYSIS_REPORT.md：宣称各模块"100% 完成、生产就绪"；
  - IMPLEMENTATION_ROADMAP.md：宣称"20 个核心模块完全缺失实现、42 个模块平台支持不完整"；
  - QUICK_REFERENCE.md：实现覆盖率 59.7%。
  交叉验证源码后判断：**QUICK_REFERENCE 最接近事实**——头文件齐全（72 个），实现约六成，且"有实现"不等于"实现与注释宣称的一致"（sdtimer 为证）。反推产物只能作为线索，一切以源码为准。

## 2. 老引擎能力盘点

| 模块 | 头文件 | 设计要点（读码结论） | 实现完整度 |
|---|---|---|---|
| 定时器 | `sdtimer.h/.cpp` | 声称分层；实际 mutex+vector 每帧全扫，支持 loop 次数/永久重复、回调出锁后执行（这点是对的） | 头≠体，功能可用但 O(n) |
| 时间 | `sdtime.h/.cpp` | 双轨：`CSDDateTime`（tm 封装，1970-2037）+ ModernDateTime/Timer（chrono 正系）；GetTickCount 49.7 天回绕注释 | 完整，双轨并存是迁移期形态 |
| 变长内存池 | `sdmemorypool.h/.cpp` | 16 档 size-class 空闲链表 + 页分配（0x80000 页），池销毁才整体归还；**注释明确"非线程安全，自己加锁"** | 完整 |
| 定长内存池 | 同上 | 页内定长单元 + per-page free 头 | 完整 |
| 对象池 | `sdobjectpool.h` + `detail/sdobject_allocator*.h` | placement-new 构造/显式析构；构造参数手写重载至 5 参（Boost PP 可到 20）；**锁策略是模板参数** | 完整 |
| 线程池 | `sdthreadpool.h/.cpp` | min/max 线程、最大挂起任务、两种终止语义（TerminateQuick/TerminateWaitJobs）；`volatile BOOL` 标志（UB 模式） | 完整 |
| 同步原语 | `sdmutex/sdcondition/sdlock/sdatomic` | 跨平台 CRT/POSIX 封装 + RAII 锁模板 | 完整（现代 C++ 下无存在必要） |
| ID 池 | `sdidpool.h` | 数组+链内 next 指针的 O(1) Alloc/Free（`next==-1` 复用为"占用"标志，精巧）；apollo `base/id_pool.hpp` 即其现代版 | 完整 |
| 队列/环形缓冲 | `sddataqueue/sdfifo/sdloopbuffer` | 跨线程投递队列、环形读写缓冲 | 完整（apollo utils 已有移植） |
| 文件/目录/映射 | `sdfile/sddir/sdfilemapping/sdcsvfile` | 同步文件 IO + mmap 封装 + CSV 解析 | 完整 |
| 共享内存 | `sdshmem.h/.cpp` | Windows file-mapping / POSIX shm 统一封装（Open/Create/Close） | 完整 |
| DB facade | `sddb.h/.cpp` | **异步命令模型**（详见 §4.1）：连接串建会话、core/max 连接池、GroupID 保序、两段式回调 | 完整（MySQL/ODBC/ADO + Mock） |
| 日志 | `sdlogger/`（7 个 cpp） | 多后端：文件/TCP/UDP，跨平台分文件 | 完整 |
| 网络 | `sdnet`（win/linux 两实现） | 回调式 ISSSession（OnRecv/Send/DelaySend/GetSendBufFree/SetBufferSize）、IOCP/Epoll、GATE 变体 | 完整 |
| 包格式 | `sdpkg.h` | 16/32 字节定长帧头：mark+len+checksum（`(len^0xBBCC)&0x88AA`），网络序 | 纯头文件 |
| 序列化 | `sddatastream.h` | 字节流读写原语 | 完整 |
| 其他 | sdcrc/sdmd5/sddes/sdstrencrypt/sdtranslate(国际化)/sdconsole/sdsysteminfo/sdgate/sdpipe/sddebugviewer | 工具与外围 | 各异 |

**结论：没有实体/属性/状态同步层。** 反推覆盖的是引擎"系统层"，游戏对象复制逻辑在原引擎的游戏层，未进入本仓库。apollo 的属性同步设计不能从这里取经（见 `docs/design/attribute-sync.md`，目标模型取 BigWorld/KBEngine）。

## 3. 与 apollo 现状对照

| 能力 | SSEngine | apollo 现状 | 判定 |
|---|---|---|---|
| 线程池 | `sdthreadpool`（min/max、手工终止语义） | `modules/base/thread_pool.hpp`（function+future，hardware_concurrency 默认） | **已有且更现代**；但缺"上限拒绝/队列上限"语义，可补 |
| 锁/条件变量 | sdmutex/sdcondition/sdlock | std::mutex/condition_variable + `utils/lock.h` | 已有且更优，勿引入 |
| 时间 | 双轨 DateTime + chrono | `modules/base/time.hpp`（chrono 正系） | 已有且更优 |
| 定时器 | sdtimer（O(n) 扫描） | **无定时器模块**（rg 全 modules 无 timer） | **缺**：按目标设计（分层时间轮）新建，见 §4.3 |
| 裸内存池 | 16 档 size-class 池 | `utils/memory_pool.h`（定长块池）+ `modules/base/memory.hpp`(ByteBuffer) | 已有；size-class 变长池思想可在网络缓冲场景复用 |
| 类型化对象池 | sdobjectpool（placement-new + MT 模板参数） | **无按类型的对象池** | **缺**：§4.2 |
| ID 池 | sdidpool | `modules/base/id_pool.hpp` / `utils/id_pool.h` | 已有（现代版） |
| 跨线程队列/环形缓冲 | sddataqueue/sdloopbuffer | `include/apollo/utils/data_queue.h` / `loop_buffer.h` | 已有（即移植件） |
| 异步 DB | sddb 两段式命令 + 连接池 + 保序 | `modules/data`：MemoryConnection/SimpleDataSource/SqlTemplate **同步 mock** | **最大缺口**：§4.1 |
| 共享内存 IPC | sdshmem/sdshm | 无（rg 无 shmem/mmap） | **缺但暂缓**：§4.4 |
| 文件映射/CSV/ini | sdfilemapping/sdcsvfile/sdiniconfig | 新 config 体系（优先级+溯源）更强；CSV 有 `utils/serializer`+serialization 模块 | 已有且更优 |
| 日志 | 多后端（文件/TCP/UDP） | `utils/logging`+`core/log`（异步、多级别） | 已有；**远程 sink（TCP/UDP 直发采集器）可作参考** |
| 网络 | sdnet（IOCP/Epoll、回调式） | `include/apollo/net`（Session/Connection/双 adapter 雏形）+ README 宣称 IOCP/Epoll | 已有骨架；老引擎的**背压可见性**与 GATE 模式值得抽象进 net-abstraction（docs/design/net-abstraction.md） |
| 包帧格式 | sdpkg 定长头+校验 | `modules/protocol`（codec/messages/nng_wrapper） | 已有；帧头字段设计可对齐（mark+len+checksum+序号） |
| 属性同步 | **无** | docs/03 草稿 + `game/attributes`（AttributeContainer 雏形） | 见 `docs/design/attribute-sync.md`（以 BigWorld 为目标） |
| 算法（CRC/MD5/DES） | sdalgorithm | `utils/crypto.h` + protocol | 已有；DES/MD5 属遗留密码学，新代码禁用 |
| 网关/管道/控制台 | sdgate/sdpipe/sdconsole/sddebugviewer | apps/gateway-app 规划中；utils/terminal | 方向已有，模块无需引入 |

## 4. 建议引入清单（按价值排序）

### 4.1 异步 DB Command 模型（价值 ★★★★★）

- **老引擎设计**（`sddb.h:253-390`）：
  - `ISSDBCommand`：`OnExecuteSql(conn)` 在 **DB 线程**执行；`OnExecuted()` 在**用户线程**的 `ISSDBSession::Run(n)` 里顺序执行；随后 `Release()` 同步回收——"重活离场、回读归主"。
  - `GetGroupId()`：同组命令保序，`-1` 不保序——把"顺序性"做成每命令显式声明，而不是全局串行。
  - `Session` 内核/最大连接数（`GetDBSession(account, coreSize, maxSize)`），同步路径带超时参数。
- **为什么 apollo 需要它**：`modules/data` 目前是 `MemoryConnection` + `SqlTemplate` 的同步形态；MMO 主循环不能内联等 DB。apollo 已有的数据文档（docs/08 Player_Data_Storage_and_BI）方向一致，缺一个线程模型定型。
- **落地形态（C++20 现代化，不搬接口）**：
  - `DbCommand = { sql/proc + 完成回调 }`，提交进 per-shard 工作队列；完成回调排回** owning 场景线程**的任务队列（与属性同步同一 tick 边界执行，保证读侧无锁）。
  - 保序键改为显式 `entityId`（同一实体的写保序、跨实体并行）——比 GroupId 更贴游戏语义。
  - 连接池 core/max + 健康检查 + 断连退避；Mock 后端保留（单测用，apollo 的 MemoryConnection 思路与此一致）。
  - 写策略与崩溃安全见 `docs/design/attribute-sync.md` §持久化（快照 + write-behind 日志），DB 线程模型复用本节。
- **成本**：模块级新建（`modules/data` 内），不动现有同步 API（可共存为 backend）。

### 4.2 类型化对象池（价值 ★★★★）

- **老引擎设计**：`CSDObjectPool<T, MT>`：free-list 分配裸内存 → placement-new 构造 → 显式析构归还；**锁策略模板参数**（单线程池传 `CSDNonMutex` 零开销）是全库最值得抄的一个设计决策。
- **apollo 缺口**：`utils/memory_pool.h` 是裸字节池，无对象语义；热路径高频对象（封包、AOI 事件、属性 delta 批次、技能投射物）目前走全局 new/delete。
- **落地形态**：C++20 不需要老引擎的宏/重载把戏——`std::pmr::monotonic_buffer_resource`（帧作用域 arena）覆盖"每 tick 临时对象"；跨 tick 常驻对象用 `free-list + std::construct_at/destroy_at` 薄封装（~100 行），线程安全同样做成模板参数或 pmr memory_resource 注入。分配点对齐接 `docs/design/attribute-sync.md` 的 per-tick arena。
- **成本**：低；先做 arena（收益最大、风险最小）。

### 4.3 分层定时器轮——按目标设计引入（价值 ★★★★）

- **老引擎现状**：头文件宣称分层，实现是 vector 扫描（`sdtimer.cpp:48-74`）——**它只是"能用"**。
- **为什么 apollo 必须有**：game loop 的 buff 到期、技能 CD、AI 思考节拍、属性回血 tick、重连超时、DB 重试——全部是定时器；无定时器模块意味着这些将来会散落成各系统私有 `next_fire_time` 扫描。
- **落地形态**：分层时间轮（如 5 层 × 64 槽，ms/10ms/100ms/1s/10s 粒度），O(1) 插入/取消；**驱动挂在主循环固定阶段**（不是独立线程——与 docs/34 §15"调度器不得进入高频 Tick 冲突"一致：调度器提供数据结构，驱动权在 game loop）；到期的回调分"主线程回调/工作线程回调"两类。SSEngine sdtimer 的"锁内收集、锁外执行"（`sdtimer.cpp:48-73`）细节正确，保留该语义。
- **成本**：中（一个独立模块 + 单测）；与 Lua 热更的关联见 §6。
- **归属（ioc-review §16.8.3-④）**：落 **modules/base 新组件（数据结构 + 单测）**——三家先例：BW TimeQueue 在最底层公共库 lib/cstdmf 且自带 unit_test（time_queue.hpp:60/:74、unit_test/test_time_queue.cpp）、KBEngine Timers 在 lib/common（timer.h:101/:108）、skynet 为核心线程编队（skynet_start.c:209-211）；模块只供 O(1) 结构与到期回调收集，驱动权按本节设计留在 game loop 固定阶段。**不落 modules/bigworld**——其为 legacy 兼容层（BigWorld.h:3-9），timer.cpp 是转发桩（:3-5，自认实现由 apollo::bw::Runtime 提供，runtime.h:40 addEntityTimer），仅作语义参考不入生产链。

### 4.4 共享内存 IPC（价值 ★★，暂缓）

- **老引擎设计**：`SDOpenShmem/SDCreateShmem` 统一 Windows mapping / POSIX shm。
- **apollo 场景判断**：BigWorld 化（base-app/cell-app 分进程）后，**同机进程间**大块只读数据（空间格子快照、监控指标、发布/订阅热点）用 shm 有真实收益；但当前 apollo 单进程为主、跨进程通信协议未定型，现在引入是过早优化。
- **建议**：列入 net-abstraction.md 的进程间通道候选项（与 Aeron 的 IPC 传输同框比较，`docs/design/net-abstraction.md` §Aeron），实现推迟到多进程落地阶段。

### 4.5 其余（明确不引入）

- sdmutex/sdcondition/sdthread/sdatomic：std 已全面替代。
- sdtime 的 `CSDDateTime` 双轨：apollo 保持 chrono 单轨。
- sdpkg 的 checksum 算法（`(len^0xBBCC)&0x88AA`）：强度不足且可预测，apollo 帧格式应使用 CRC32 + 序号（见 attribute-sync.md §协议）。
- DES/MD5：遗留密码学，仅兼容旧数据时用。
- sdtranslate 国际化/sdconsole/sddebugviewer：与 apollo 现有规划重叠度低、收益小。

## 5. 反推暴露的教训（踩坑记录）

1. **文档与实现脱节是常态**：sdtimer.h 的"分层定时器"注释 vs vector 扫描实现；FINAL_ANALYSIS_REPORT 的"100% 生产就绪" vs ROADMAP 的"20 模块无实现"。→ apollo 的约束：模块 README 必须声明"已实现/部分/规划"三态，CI 里跑能力清单断言（文档也是代码）。
2. **`volatile BOOL` 做线程标志**（`sdthreadpool.h:140-141`）：C++ 中 volatile 无同步语义，属 UB 边缘。→ 新代码一律 `std::atomic`。
3. **注释式线程安全**（"remember to lock"，`sdmemorypool.h:29-41`）：把正确性责任转嫁给每个调用点，规模一大必漏。→ 线程策略进类型（sdobjectpool 的 MT 模板参数是对的）或文档化为"单线程类型"并在调试模式加断言。
4. **裸指针 + `Release()` 手工生命周期**（ISSDBRecordSet/ISSDBSession/ISSBase AddRef/QueryRef）：COM 风格在无 GC 语言里把每个返回值都变成泄漏点。→ unique_ptr/shared_ptr 边界清晰化。
5. **Windows 中心 API 面向迁移的代价**：`SSAPI/CHAR/BOOL/匈牙利命名` 与手工平台宏——反推重建时这些正是最难对齐的部分。→ apollo 坚持标准库优先、平台差异关进 adapter（net-abstraction.md 的分层动机之一）。
6. **定长头+校验是老而正确的**（sdpkg 的 mark+len 结构方向正确，算法强度不足）：apollo 的帧格式设计可以站在它肩膀上加序号与 CRC。

## 6. 与 Lua 热更规划的交集

- **脚本资源热加载的读取底座**：文件变更检测（老引擎 sdfile/sdfilemapping 的 mtime 思路，apollo 已有 `utils/config/FileWatcher.h` inotify/poll 实现）+ `loop_buffer` 双缓冲拷贝 + **§4.3 定时器轮的低频扫描**（开发期 200ms，线上关闭）= 热加载管线三件套；加载/编译在工作线程，主线程 tick 边界原子切换（详见 `docs/design/scripting-lua.md` §热替换协议）。
- **异步 DB 模型 ↔ 脚本异步**：§4.1 的"提交→回主线程回调"与 Lua 侧的协程/yield 模型天然对齐——脚本发起 `db.query()` 即挂起协程，完成回调在 tick 边界 resume；这是把 DB 线程模型同时暴露给 C++ 与 Lua 的同一套底座。
- **对象池/arena ↔ 脚本 GC**：per-tick arena 承接 C++ 侧高频临时对象后，Lua 侧只需管理自己的增量 GC 步长，两侧互不拖累（scripting-lua.md §沙盒与性能预算）。

---

*阅读基线：SSEngine @ 工作副本（反推重建仓库）；apollo main @ 35a9c528。2026-09-29 补 §4.3 归属行（ioc-review §16.9.4/§16.10.1 粘贴）。*
