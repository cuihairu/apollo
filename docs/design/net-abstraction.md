# Apollo 网络层抽象设计（net-abstraction）

> 状态：设计稿（评审中）。目标：给游戏逻辑一个"薄"的网络 API，把连接管理、编解码、重连、背压全部下沉；并回答"自研薄封装 vs 引入 Aeron"的边界问题。与 `docs/design/attribute-sync.md`（QoS 通道/预算）、`docs/design/scripting-lua.md`（脚本只见三件套）、`docs/analysis/ssengine-reference.md`（sdnet 老设计）互为引用。

---

## 执行摘要

1. **游戏逻辑只看到四个动作**：`send / subscribe / state / close`——握手、帧定界、心跳、重连续传、水位、线程迁移全部下沉；回调固定落在 owning 场景线程（与 attribute-sync §10、scripting-lua §2 的单写者纪律同一条线）。现有 `include/apollo/net/session.h` 是"系统库形态"（`onRecv` 裸字节、返回已处理字节数，session.h:59），距"游戏可用"差一整个会话层。
2. **删除 sdnet_adapter.h**：它在 apollo 仓库里**手写伪造了 SSCP 命名空间与 ISSBase/ISSConnection 接口**（`include/apollo/net/adapters/sdnet_adapter.h:12-80`，`#define APOLLO_USE_SDNET` 无任何链接真 SSEngine 的构建配置）——名为适配器实为空壳，与 architecture-review 里 fake-fruit 是同一类问题。sdnet 的真精华（DelaySend 跨线程投递、GetSendBufFree 水位、GATE 网关模式）以设计点形式收进本设计。
3. **Aeron 结论（已关闭，architecture-review §15.2）：客户端不用，进程间总线自研。** Aeron 定位是 UDP 单播/多播与 IPC 的机器间消息传递（term buffer、offer/poll + BACK_PRESSURED、flow control + NAK、Archive 回放、Raft 集群），**不做海量 TCP/WS 玩家长连接**——客户端路径自研薄封装；进程间总线也已定**自研**（§15.2：nng 退役、禁止第二套进程间通信并存），Aeron 降级为语义蓝本（§5.2 吸收清单照旧）。
4. **背压是一等公民**：四级水位（ok/soft/hard/cut）+ `trySend` 显式返回码（ACCEPTED/BACK_PRESSURED/TRIMMED_LOW）——"不排队、不阻塞、把决策还给调用方"即 Aeron `offer()` 哲学；丢弃顺序由 attribute-sync §5 的优先级体系决定（低重要性/远距离先 trim）。
5. **QoS 四通道**（movement 不可靠 / attributes 可靠 / events / control）跑在同一会话上，与属性同步的 token-bucket 预算形成两级独立控制："预算"决定该发多少，"水位"决定还能不能发——都在单写者线程决策，无锁。
6. **NNG wrapper 已定退役**（architecture-review §15.2 退役清单）：`modules/protocol/nng_wrapper` 随五项退役一并删除，避免两套进程间通信并存（与"四套配置系统"同构的重复问题，不再制造第三处）。
7. **内部网络层按演进阶梯交付（§5.6，2026-09-29 增补）**：自研的意义 = 拥有**可持续优化的内核**（Mercury 同型——BigWorld 内部网络层演进二十年而非一次性交付）——M0 语义定型（P1 随本设计）/ M1 最小正确内核（P3）/ M2+ 持续优化（无截止，按需小批）；优化只发生在层内，消费方零感知。

---

## 1. 现状盘点（读码结论）

| 部件 | 位置 | 现状 | 判定 |
|---|---|---|---|
| Connection/Listener/EventLoop | `include/apollo/net/connection.h` `listener.h` `event_loop.h` | IO 抽象骨架，方向正确 | **保留为 L0 原型** |
| Session | `include/apollo/net/session.h:37-128` | 回调式：`onRecv(data,len)` 裸字节 + 返回处理字节数；send 直通 Connection；心跳仅 get/set 时间戳，无实现 | 接口形态不对（裸 TCP 语义漏给上层）；**重设计为 L2** |
| native_adapter | `include/apollo/net/adapters/native_adapter.h` | 自研 epoll 骨架 | 保留为 L0 唯一真实现 |
| sdnet_adapter | `include/apollo/net/adapters/sdnet_adapter.h:8,12-80` | **伪 SSCP**：`#define APOLLO_USE_SDNET` 后手写 `namespace SSCP`、ISSBase/ISSConnection/版本结构/错误码——仓库无任何配置链接真 SSEngine | **删除**（见 §6） |
| HTTP/WebSocket/RPC | `include/apollo/net/http/`、`websocket.h`、`rpc.h` | 外围能力雏形 | 与本设计正交，另行处理 |
| protocol 模块 | `modules/protocol`（codec/messages/nng_wrapper/socket） | NNG 封装 + 自研消息编码；**全仓库无 .proto 文件** | codec 并入 L1；nng_wrapper 见 §6 决策 |
| SSEngine sdnet | `/home/cui/workspaces/SSEngine`（对照） | `Send/DelaySend`（跨线程投递）、`GetSendBufFree`（水位可见）、`SetBufferSize`、GATE 变体 | 思想收编（§5.4） |

核心判断：**apollo 不缺 IO 层零件，缺的是 L1/L2**——帧格式、会话语义（seq/ack/心跳/重连）、通道与背压。这三样是游戏网络层与"网络库"的分界线。

## 2. 目标形态：游戏逻辑看到的 API

```cpp
// 游戏系统（ECS 场景系统、属性同步、Lua 绑定）只依赖这一个句柄
class GameConnection {
public:
    // 唯一发送入口。返回码即背压决策（不排队、不阻塞）：
    //   ACCEPTED        已入队
    //   TRIMMED_LOW     入队，但同通道低优先级消息被丢弃腾位（结果统计上报）
    //   BACK_PRESSURED  拒绝（hard/cut 水位），调用方自行降级（如跳过本次同步帧）
    SendCode  send(MsgPtr msg, Channel ch, Priority p);

    void      subscribe(MsgId id, MsgHandler h);   // 回调固定在 owning 场景线程
    SessionState state() const;                    // Handshaking/Active/Resuming/Closed
    void      close(std::string reason);
    // 重连对上层唯一可见的痕迹：Resuming 成功后 resume 回调带 (missing_seq 范围)，
    // 由上层决定补发（属性同步走 AttrSync 快照补发，见 attribute-sync §6）
};
```

隐藏清单（对游戏逻辑不可见）：连接建立与握手、加密、帧定界与 CRC 校验、心跳保活、seq/ack 与重连续传、水位与 trim、跨线程迁移（send 可从任意线程调，内部入 MPSC 环）、Lua 侧更只看到 `apollo.net` 的三件套（scripting-lua.md §7）。

## 3. 分层架构

```
L3  Logic Facade      GameConnection（§2）——游戏逻辑/Lua 唯一入口
L2  Session           seq/ack、心跳、重连续传(resume token)、四通道队列、水位机
L1  Framing/Codec     帧头(16B: magic+len+seq+crc32+ch/flags) + protobuf + zstd(>256B)
L0  Transport         adapters: epoll/io_uring(native) | IOCP | Aeron | IPC | (测试: loopback)
```

### L1 帧格式

对齐 sdpkg 的"定长头+校验"方向、修正其强度：`magic(2B) | header_len/ver(1B) | channel+flags(1B) | seq(4B) | len(4B) | reserved(2B) | crc32c(2B header 头校验)`，payload 校验 crc32c 随帧尾（或 len 字段 + 尾部 4B CRC）。变化点 vs sdpkg：加 **seq**（重连与乱序检测的基础）、CRC32C 替换 `(len^0xBBCC)&0x88AA`（ssengine-reference.md §4.5 判定：可预测、强度不足）。

### L2 会话语义

- **seq/ack**：每通道独立 seq；ack 压缩为累积确认 + 可选 bitmap（SACK 思想）随上行捎带。
- **心跳**：L1 层 ping/pong 帧，Session 层维护 RTO；超时 → Closed(Timeout)，连接清理。
- **重连续传**：断线时 Session 保存未 ack 的 reliable 队列 + resume token（TTL）；重连握手带 token，服务端比对 `acked_seq`——**与属性同步 ViewerState.acked_seq 是同一个模型的两个实例**（attribute-sync §3）；过期则全新会话，属性走 enter-view 快照，天然兜底。
- **四通道队列**：每会话 4 条队列（§4），单写者（场景线程）入队、IO 线程 flush，MPSC 环（复用 `include/apollo/utils/loop_buffer.h`、`data_queue.h` 移植件）。

### 线程模型

```
IO 线程（1-2 个）：epoll/io_uring 事件 → 读入 per-conn 缓冲 → L1 解帧/校验/解压
                   → payload 按 (conn → entity) 投递到 owning 场景线程任务队列
场景线程（单写者）：处理上行 → 改属性/状态（attribute-sync §10 六阶段）→ 产出下行
                   → send() 入 MPSC 环（含水位判定，零锁）
IO 线程 flush：按水位预算从环上取帧 writev
```

关键纪律：**回调只发生在场景线程**（游戏逻辑无锁的前提）；**IO 线程永不持游戏数据锁**（只搬运字节）；发送方向游戏线程是生产者、IO 线程是唯一消费者。

## 4. QoS 通道与背压

### 4.1 四通道（与 attribute-sync §5.3 对齐）

| 通道 | 可靠性 | 顺序 | 典型负载 | 拥塞行为 |
|---|---|---|---|---|
| `movement` | 不可靠 | 无（带时间戳取最新） | 位置/朝向/速度 | **只发最新**：直接覆盖未发送的同实体旧帧 |
| `attributes` | 可靠 | 有 | 属性 delta/快照 | 只允许 trim 低优先级实体批次（§5 优先级），不可跳号 |
| `events` | 可靠 | 有 | AOI 进入/离开、战报、飘字 | 不丢，满了压水位 |
| `control` | 可靠 | 有 | 握手/心跳/ack/resume | 绝不丢，独立小队列保证饿不死 |

### 4.2 四级水位与行为表

| 水位 | 判据（每通道发送环占用） | 行为 |
|---|---|---|
| ok | < 50% | 全量发送 |
| soft | 50–75% | movement 启用"只发最新"；attributes 按 Priority::LOW 开始 trim |
| hard | 75–95% | trySend 对 LOW/NORM 返回 TRIMMED_LOW/BACK_PRESSURED；触发慢路径：上报监控 + 属性预算自动降档（token bucket 补给率下调，attribute-sync §5） |
| cut | ≥ 95% | 该会话 reliable 队列冻结新入队（控制帧除外）；持续 N 秒 → 断开（客户端自己会重连 + resume） |

- **水位查询标准化**：`Watermark::query(conn, ch) -> level` 取代 sdnet `GetSendBufFree()` 的裸字节数——把老引擎"水位可见"思想收进类型化 API（ssengine-reference.md §3 判定）。
- **两级控制正交**：attribute-sync 的 token bucket 决定"本 tick 该发多少字节"（节流），本层水位决定"还能不能入队"（背压）；前者是主动整形、后者是被动熔断，相遇在 hard 水位（预算降档）。

## 5. Aeron 研究与取舍

### 5.1 机制速览（源码/文档要点）

- **模型**：Publication（发送端，可多活）↔ Subscription（接收端）；一条 Subscription 收到的每个发送端连接叫 Image，每 Image 一个 sessionId——多路复用与流隔离的原语。
- **传输**：term buffer = 每流三段式环形日志缓冲（顺序写、索引页），单写者写、读取方扫描——**无锁的前提是"每 buffer 单写者"**，与 apollo 场景线程纪律同构。
- **背压**：`offer()` 非阻塞，返回 BACK_PRESSURED/ADMIN_ACTION/封闭错误——不排队不阻塞，决策还给调用方；接收端 flow control（min/median 策略聚合多订阅者窗口）+ NAK 请求重传，reliable/unreliable 两种流模式。
- **外围**：Media Driver 独立进程或内嵌（embeddedMediaDriver）；IPC 传输走共享内存（同机进程间零拷贝）；Archive 模块录制流可回放；Cluster 模式 Raft 复制状态机。Apache 2.0，C/C++ 客户端成熟。

### 5.2 吸收什么（无论是否引入库）

| Aeron 概念 | 进 apollo 的落点 |
|---|---|
| offer() 返回码哲学 | §2 `trySend` 三返回码——上层显式处理拥塞，禁止"默默排队等死" |
| term buffer 单写者环形分段 | 发送环/接收环的数据结构蓝本（自研 MPSC 环直接按此设计） |
| per-stream sessionId 隔离 | §4 四通道即 apollo 的流隔离粒度 |
| flow control 聚合窗口 | 多观察者下行聚合：慢观察者拉低整批发送速率（AOI 广播的 per-viewer 预算，attribute-sync §5） |
| IPC 共享内存传输 | 进程间同机通道候选（与 sdshmem 同框，ssengine-reference.md §4.4） |
| Archive 回放 | P3：跨服消息审计/故障重放（可选） |

### 5.3 结论：哪里用、哪里不用

| 路径 | 决策 | 理由 |
|---|---|---|
| 客户端 ↔ 服务器（海量 TCP/WS 长连接，万级 conn） | **自研薄封装**（本设计 L0–L3） | Aeron 不做 TCP 长连接接入；media driver 每连接开销、UDP 玩家侧不可靠网络适配、运维复杂度全不匹配。玩家路径的问题是"每连接会话语义"，这正是 Aeron 刻意不做的层 |
| 进程间总线（gateway↔game↔world↔db-proxy，机器间+同机） | **自研（已定，architecture-review §15.2）**，语义照 §5.2 抄（term buffer 单写者分段/流控聚合/NAK/offer 返回码） | nng 退役后禁止再引第二套进程间通信；自研蓝本现成——§5.2 吸收清单 + §5.5 Mercury filter 双族 + udp_channel 窗口重传 |
| 同机大块只读共享（空间快照/指标） | sdshmem 类方案，P3 随进程间总线同批定案（§6 决策行/§7 P3 已接线） | 见 ssengine-reference.md §4.4 |

### 5.4 与 SSEngine sdnet 对照

| sdnet | 本设计对应 | 关系 |
|---|---|---|
| `DelaySend`（跨线程投递） | 场景线程 → IO 线程 MPSC 环 | 同思想，环结构按 term buffer 蓝本现代化 |
| `GetSendBufFree` | `Watermark::query` 四级水位 | 裸字节数 → 类型化决策 API |
| `SetBufferSize` | 会话建立参数（内部） | 保留，对上层隐藏 |
| GATE 变体 | gateway-app（apps/gateway-app 规划） | 方向一致；网关与本层的接口即 §2 API |
| sdpkg 帧头 | L1 帧格式 | 加 seq + CRC32C（§3） |

### 5.5 Mercury filter 双族 → L1 帧管线蓝本（2026-09-28 增补，源证见 architecture-review §16.7.1）

BigWorld Mercury 的 filter 是**已运行二十年的帧管线插件体系**，四点收编：

- **双族接口**（packet_filter.hpp:37 send/:48 recv/:56 maxSpareSize；stream_filter.hpp:46 writeFrom/:55 readInto）统一为 apollo 的 `FrameFilter`：`Reason encode(Frame&) / decode(Frame&) / reserve()`——Reason 表达"放行/截断/还需要更多字节"，`reserve()` 让压缩/加密 filter 预告膨胀量，避免逐帧重分配。
- **WS 实证「禁止第五套网络栈」**：BigWorld 的 WebSocket 是 stream filter 而非独立栈（packet_filter/stream_filter 双族 + mutable_stack 可配）——apollo 的 WS 沿同一插件位，不新起传输层。
- **filter 栈随契约声明**：帧头 `ver` 字段 + 契约 messages.xml 的通道声明（sdk-contract §2.3）→ 生成器产出 L1 装配代码，filter 顺序跨端一致由契约锁死，不跑运行期字符串配置。
- **边界纪律**：filter 只做字节↔字节变换（压缩/加密/混淆）；seq/ack/重传窗口留在 L2 通道内核——BigWorld 的窗口重传在 udp_channel 而非 filter 里，这是"插件位"与"会话核心"的分界。
- **归属与前置条件**（architecture-review §16.8.3-①）：FrameFilter 接口与内置插件落**收敛后的 modules/net**——先例：BW filter 与通道同库、构造注入（udp_channel.hpp:81/:139-140）；protocol（生成 codec）经 L1 codec 槽位注入，apps/gateway-app 只装配不实现。**前置条件 = C-29 四套网络树收敛**（architecture-review §12.2 四套盘点、§15.2「禁止第五套」纪律）——filter 不落现存四套中任何一套的原样，否则 filter 链即第五处网络代码。

### 5.6 内部网络层的演进策略：先立内核、持续优化（2026-09-29 增补）

BigWorld 的 Mercury 是**演进了二十年的内部网络层**，不是一次性交付的库——filter 双族（§5.5）、udp_channel 窗口重传等能力是多年逐步叠加的（源证 architecture-review §16.7.1），游戏层 API 在这期间保持稳定。apollo 的自研决定（§15.2）要学的正是这个形态：**自研 = 拥有可持续优化的内核**，不是「一次写完对标二十年」——引外部库反而把优化节奏交给别人。apollo 的内部网络层 = 本设计的 L0-L3（客户端/单进程路径）+ 进程间路径，**同一套语义、同一个层**（§2 四动作、SendCode 返回码、四通道、水位机、FrameFilter 槽位两路通用），差异只在传输与拓扑。演进阶梯：

| 阶段 | 交付 | 性能口径 |
|---|---|---|
| **M0 语义定型**（P1，随本设计落地） | 四动作 API、SendCode 三返回码、四通道、水位机、FrameFilter 槽位——在客户端/单进程路径落地即定型；进程间路径消费**同一套语义**，不开第二 API 面 | 无（接口层，定型的是语义不是实现） |
| **M1 最小正确内核**（P3 首批） | 进程间段：帧定界 + contract_route 按 id 分发（sdk-contract §10.3）+ 背压水位 + 单播拓扑；同机段 shm 形态同批定（§6 决策行——进不进首版看同机部署有无） | **正确性优先，不设性能指标**——初期内部流量小，过早优化无对象 |
| **M2+ 持续优化**（无截止，按需小批） | 窗口重传（udp_channel 蓝本）、流控聚合（Aeron 蓝本 §5.2）、filter 插件族扩充（加密/审计）、bundle 延迟聚合（一 tick 攒包一次冲刷——attribute-sync §7.1「每客户端每 tick 至多一帧」即其同型，推广到进程间）、同机 shm 通道实现（sdshmem 候选，G-2 镜像流——形态 M1 定、实现可后置） | **每项先有基准再动手**，独立小批、可回退 |

两条纪律保证「可持续」：

- **优化只发生在层内**：消费方（属性同步/脚本/业务）只依赖 §2 四动作与通道语义——内核重写、传输替换、filter 增删对它们零感知。Mercury 二十年演进不改游戏层 API 是同一事实；这是「游戏逻辑看不到网络」（§2 隐藏清单）的长期红利，也是它存在的目的。
- **每级有各自的验收对象**：M0 验收语义冻结（接口评审 + 契约入 sdk-contract）；M1 只验收正确性（分发不丢不重、背压可见、断连可检）；M2+ 每项优化以基准数据准入——同时防「为优化而优化」与「假装在演进」两种失败形态。

与 §7 分期的对应：原「P3 总线落地」口径改为「**M1 上线**」；M2+ 不占分期里程碑——它们是内部网络层的**常态工作**，不是路线图节点。

## 6. 决策清单（保留/删除/引入）

| 对象 | 决策 | 依据 |
|---|---|---|
| Connection/Listener/EventLoop | **保留**为 L0 原型，接口收敛为"事件源 + writev + 水位" | 方向正确，缺的是上两层 |
| `sdnet_adapter.h` | **删除** | 伪 SSCP（sdnet_adapter.h:12-80），从未链接真库，空壳代码；精华已提炼为设计点（§5.4） |
| `session.h` 现接口 | **重设计**（不保留回调形态） | onRecv 裸字节把 TCP 语义漏给游戏逻辑，是本设计要消灭的头号泄漏点 |
| `modules/protocol` codec/messages | **并入 L1**，codec 迁到 protobuf（与 attribute-sync §7、sdk-contract 同一契约源） | 全仓库无 .proto，手写编码无法支撑多端 SDK |
| `nng_wrapper` | **已定删**（architecture-review §15.2 退役清单五项之一） | 两套进程间通信并存即重复；决策已闭环 |
| 四级水位/四通道/resume | **新建**（L2） | apollo 完全没有；属性同步设计的直接依赖 |
| Aeron | **已定不引入**；§5.2 语义吸收清单与 §5.5 filter 蓝本保留 | §5.3 已关闭为自研（architecture-review §15.2） |
| FrameFilter 体系 | **新建**于收敛后的 modules/net（四套收敛为前置条件） | architecture-review §16.8.3-①：BW 同库 + 注入式装配先例（udp_channel.hpp:81/:139-140） |
| sdshmem 类同机共享内存通道 | **暂缓**（P3 随进程间总线同批定案；引设计不引代码——与 Aeron 同等处理） | 只承载**同机大块只读共享与点对点镜像流**（空间格子快照/监控指标/G-2 热备镜像流），**不做通用消息总线**（architecture-review §15.2 禁第二套 IPC 并存的边界照旧）；ssengine-reference §4.4 + Aeron IPC 同框（§5.2） |

## 7. 分期落地

- **P1**：L1 帧格式 + L2 会话基础（seq/ack/心跳/四级水位）+ native adapter 收敛为唯一 L0；GameConnection Facade 上线，属性同步 P1 依赖本层 attributes 通道。
- **P2**：重连续传（resume token）+ movement 通道"只发最新" + 与属性预算的 hard 水位联动降档。
- **P3**：内部网络层 **M1 最小正确内核**上线（§5.6 演进阶梯——进程间段：帧定界 + contract_route 按 id 分发 + 背压水位 + 单播拓扑，正确性优先、不设性能指标；蓝本：§5.2 语义 + §5.5 Mercury filter 双族；同机段加评估 sdshmem 类共享内存通道——只承载大块只读共享与点对点镜像流（快照/指标/热备镜像），不做通用总线，§15.2 边界照旧，随内核同批定案，引设计不引代码：ssengine-reference §4.4、Aeron IPC 传输同框；窗口重传/流控聚合/bundle 聚合归 **M2+ 持续优化**，不占本里程碑）、网关模式（gateway-app 接入）、Archive 类消息审计；运维观测通道两截落位（architecture-review §16.8.3-⑤）：检测原语（per-scene 心跳版本号/队列水位/实体计数/帧耗时）内嵌 owning 模块并经 control 通道上行，聚合工具归 apps/（依赖面 = modules/runtime 的 ConsoleEvent/IConsoleEventSource，application_host.hpp:20/:28；先例 skynet debug_console/monitor、BW server/tools/{bw_profile,message_logger}）——模块零依赖 apps，ops 工具只触只读自省接口。

### P3 前置设计：进程编队与服务发现 / 备份容灾（2026-09-29 补，architecture-review §16.4 G-1/G-2）

P1-P2 单进程阶段本层零落地；此节先把 P3 的两个前置形态定下来，防止到时拍脑袋（原六份设计文档的空白，登记于 architecture-review §16.4）。

**现状声明（显式）**：apollo 单进程阶段**无进程级高可用**——崩溃安全 = write-behind journal 数据不丢（attribute-sync §8.2）+ 重启拉起；backup/接管/迁移为零起步。这是分期不是遗漏。

**G-1 进程编队与服务发现**（进程如何被发现、如何感知彼此死亡）：

- 先例一 BigWorld：每机一个 bwmachined 守护进程，birth/death 通知经 machine_guard 协议订阅/广播（machine_guard.hpp:496-497/:609-613/:882-883）；cellappmgr/baseappmgr/dbappmgr 消费生死事件做编队决策，cellappmgr 兼负载平衡（cellappmgr.hpp:43/:192-194）。
- 先例二 KBEngine：无守护进程，machine 以 UDP 广播应答发现（machine.cpp:646-670，KBE_PORT_BROADCAST_DISCOVERY）——更轻，但「机器死了谁来报」无解。
- apollo 形态（P3 定稿口径）：两层并存——单机 machined 式守护（本机进程生死/拉起/崩溃上报）+ UDP 广播发现（跨机拓扑发现），分别取两先例长处；与 architecture-review §15.2 自研纪律对齐，不引 etcd/consul 类外部协调服务——游戏服进程拓扑小、变更低频，守护+广播的最终一致够用，外部强一致依赖换不来对等收益。控制面复用 §4.1 control 通道的进程间延伸：编队事件（进程加入/退出/机器死亡）作为 control 事件进各进程轮询源——不开新通道体系。

**G-2 备份与宕机接管**（BigWorld 全套先例；apollo P3 骨架取两件）：

- 先例件：baseapp 热备分帧发送（backup_sender.hpp:52-61）+ 一致性哈希备份链（backup_hash/backup_hash_chain——备机按链持续追主机实体状态流）+ reviver 协调接管（主机死亡后备机把镜像实体升级为权威；cellapp 死则 cellappmgr 在幸存 CellApp 重建 cell）+ secondary db 任务族 + dbappmgr 扩缩容哈希再分布（dbappmgr.cpp:472/:637）。KBEngine 对照：无此层（architecture-review C-51/§16.4）——宕机靠实体最后一次归档，窗口 = 归档周期。
- apollo P3 最小骨架：**backup-hash 链 + reviver 两件先行**；secondary db/动态扩缩容推迟到多 cell 稳定运行后。
- **备份粒度对齐单写者纪律**：热备流 = PersistJournal 的只读镜像消费（attribute-sync §8.2 journal 的第二消费者），不另起一套备份协议——备机 ack 的 journal 位点即接管起点，与属性 seq 语义（ViewerState.acked_seq 同模型）天然衔接；接管 = 备机在 journal 位点重放后于 tick 边界切换为权威写者（复用 attribute-sync §10.2 停机序列的镜像路径：先停旧主的写入认定，再切权）。镜像流传输形态 P3 定：**同机部署候选 = sdshmem 类 shm SPSC 环**（主机单写 journal 追加、备机单读消费——正是 §6 决策表「点对点镜像流」的典型场景；ack 仍走 control 通道），跨机则随进程间总线。

## 8. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| attribute-sync.md | 四通道即其 §5.3 QoS 分割；水位 hard 档触发其 token bucket 降档；resume 依赖其 ViewerState.acked_seq 模型；帧压缩同 §7（zstd>256B） |
| scripting-lua.md | 脚本只见 send/subscribe/回调三件套；异步回调按 tick 边界 resume 协程；背压/重连对脚本不可见 |
| sdk-contract.md | 契约文件是 L1 编码与各端 SDK 的同一来源；帧格式写入契约（各端插件据此实现帧定界） |
| ssengine-reference.md | DelaySend/GetSendBufFree/GATE/sdpkg 四点收编（§5.4）；sdnet_adapter 反例（伪命名空间）与 fake-fruit 同类 |
| architecture-review.md | 网络适配器注册走 core::di 启动期装配（编译期类型键），不进旧字符串容器；adapters 的存在形态=链接期选择，非运行期字符串切换 |
| xml-generation.md | 帧头 ver → filter 栈声明（16.7.1）由契约生成器装配（用途④）；messages.xml 的通道 enumeration 由其四层漏斗第①层校验 |

---

*基线：apollo main @ 35a9c528（include/apollo/net、modules/protocol 读码）；Aeron 参考其官方仓库文档与 C++ 客户端源码概念；SSEngine sdnet 读码对照。2026-09-28 同步修订（②）：Aeron/nng 决策关闭（architecture-review §15.2）、§5.5 Mercury filter 双族蓝本增补（§16.7.1 源证）。2026-09-29 增补（③）：filter 归属/前置条件与决策表 FrameFilter 行（architecture-review §16.9.2 粘贴）、§7 P3 观测通道行（§16.9.5 粘贴）、新增「P3 前置设计」节（G-1/G-2，architecture-review §16.4 空白的补设计）。同日增补（④）：sdshmem 同机共享内存由「暂缓观察」升格为 P3 正式候选（与 Aeron 同等处理——引设计不引代码、随自研总线同批定案）——§5.3 行改口径、§6 新增决策行、§7 P3 总线段加评估、G-2 热备镜像流补同机传输候选（ssengine-reference §4.4 既有登记的接线）。同日增补（⑤）：新增 §5.6「内部网络层的演进策略」——自研 = 拥有可持续优化的内核（Mercury 二十年演进同型）：M0 语义定型（P1）/ M1 最小正确内核（P3，正确性优先不设性能指标）/ M2+ 持续优化（无截止、每项先有基准）；摘要 7 与 §7 P3 里程碑口径同步（「总线一次性落地」→「M1 上线」，窗口重传/流控聚合/bundle 聚合移出路线图归常态优化）。*