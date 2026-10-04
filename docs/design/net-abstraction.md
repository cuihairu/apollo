# Apollo 网络层抽象设计（net-abstraction）

> 状态：设计稿（评审中）。目标：给游戏逻辑一个"薄"的网络 API，把连接管理、编解码、重连、背压全部下沉；并回答"自行开发薄封装 vs 引入 Aeron"的边界问题。与 `docs/design/attribute-sync.md`（QoS 通道/预算）、`docs/design/scripting-lua.md`（脚本只见三件套）、`docs/analysis/ssengine-reference.md`（sdnet 老设计）互为引用。

---

## 执行摘要

1. **游戏逻辑只看到四个动作**：`send / subscribe / state / close`——握手、帧定界、心跳、重连续传、水位、线程迁移全部下沉；回调固定落在 owning 场景线程（与 attribute-sync §10、scripting-lua §2 的单写者纪律同一条线）。现有 `include/apollo/net/session.h` 是"系统库形态"（`onRecv` 裸字节、返回已处理字节数，session.h:59），距"游戏可用"差一整个会话层。
2. **删除 sdnet_adapter.h**：它在 apollo 仓库里**手写伪造了 SSCP 命名空间与 ISSBase/ISSConnection 接口**（`include/apollo/net/adapters/sdnet_adapter.h:12-80`，`#define APOLLO_USE_SDNET` 无任何链接真 SSEngine 的构建配置）——名为适配器实为空壳，与 architecture-review 里 fake-fruit 是同一类问题。sdnet 的真精华（DelaySend 跨线程投递、GetSendBufFree 水位、GATE 网关模式）以设计点形式收进本设计。
3. **Aeron 结论（已关闭，architecture-review §15.2）：客户端不用，进程间总线自行开发。** Aeron 定位是 UDP 单播/多播与 IPC 的机器间消息传递（term buffer、offer/poll + BACK_PRESSURED、flow control + NAK、Archive 回放、Raft 集群），**不做海量 TCP/WS 玩家长连接**——客户端路径自行开发薄封装；进程间总线也已定**自行开发**（§15.2：nng 退役、禁止第二套进程间通信并存），Aeron 降级为语义蓝本（§5.2 吸收清单照旧）。
4. **背压是一等公民**：四级水位（ok/soft/hard/cut）+ `trySend` 显式返回码（ACCEPTED/BACK_PRESSURED/TRIMMED_LOW）——"不排队、不阻塞、把决策还给调用方"即 Aeron `offer()` 哲学；丢弃顺序由 attribute-sync §5 的优先级体系决定（低重要性/远距离先 trim）。
5. **QoS 四通道**（movement 不可靠 / attributes 可靠 / events / control）跑在同一会话上，与属性同步的 token-bucket 预算形成两级独立控制："预算"决定该发多少，"水位"决定还能不能发——都在单写者线程决策，无锁。
6. **NNG wrapper 已定退役**（architecture-review §15.2 退役清单）：`modules/protocol/nng_wrapper` 随五项退役一并删除，避免两套进程间通信并存（与"四套配置系统"同构的重复问题，不再制造第三处）。
7. **内部网络层按演进阶梯交付（§5.6，2026-09-29 增补）**：自行开发的意义 = 拥有**可持续优化的内核**（Mercury 同型——BigWorld 内部网络层演进二十年而非一次性交付）——M0 语义定型（P1 随本设计）/ M1 最小正确内核（P3）/ M2+ 持续优化（无截止，按需小批）；优化只发生在层内，消费方零感知。
8. **进程间连接设施对上层开放（§5.7，2026-09-29 补）**：`InterServerLink` = 进程间稳定连接的一等公民公开设施——连接器（Mercury TCPConnectionOpener 蓝本：非阻塞 connect + 超时 + errno 失败分类，机制不带重试）、统一重连管理（指数退避 + G-1 编队事件联动——收敛 BW LoggerEndpoint 各消费者自写重连的反面）、断连 in-flight 语义按 invoke_mode 分（OneWay 丢 / ReliableEvent 有界排队续传 / RequestReply 超时不复活）、对端重启以 authority_epoch 区分（≠ 断线重连）、per-Link 背压不 trim；跨服业务与框架内部（RemoteEntityCall/G-2 镜像流/collector push）消费**同一 API 族**——不开放裸 socket、不开放契约外消息（Mercury InterfaceMinder 单表病根不学）。
9. **L0 传输模型两族分析落盘（§5.8，2026-09-29 补）**：就绪（reactor：epoll 基线）与完成（proactor：IOCP/io_uring）两族的接口/运行模式差异定形——L0 接口按「**发送环提交语义**」（post_send/on_send_complete/post_recv/on_recv）定形而非「事件源 + writev」：reactor 适配器内部翻译成「就绪 → 环取数 writev → 同步返回即完成」，completion 适配器直投 SQE/OVERLAPPED——两族通吃且水位机判据（环占用）零改动、取消语义统一 ABORTED。三框架先例全 reactor（本轮实读：BW select-only / KBE epoll+select / skynet epoll+kqueue），完成模型列 M2+ 基准准入；**IOCP 不进路线图**（服务器 Linux-only，`_WIN32` 分支仅为可编译性垫片）。仓内三处 L0 现状一并盘点：B 栈 poll（底座）、栈 A IOCP+epoll 骨架（epoll 建而事件循环未消费——C-18 死 inotify 同族）、ipc 树 IoMultiplexer 完成模型接口（vector 拷贝语义，不学）。
10. **上行/安全/出站三面收口（2026-09-30 增补）**：上行 per-session 限流 + violation_score 风控单桶（§4.3——下行预算的镜像，超限帧不进场景线程）；客户端通道安全 = CryptoFilter（压缩后位次）+ X25519/AEAD/HKDF 握手 + **OpenSSL 单一 crypto 源**（§5.9——防网络第三方，不防持客户端的玩家）；出站 HTTP = curl 收口 + Drogon 分支删除 + 同步 API 禁场景线程直调 + 目标白名单（§5.10）；观测接出「三禁」与单一 exporter/tail/trace 子集定案于 logging.md §5.1（#10）。

---

## 1. 现状盘点（读码结论）

| 部件 | 位置 | 现状 | 判定 |
|---|---|---|---|
| Connection/Listener/EventLoop | `include/apollo/net/connection.h` `listener.h` `event_loop.h` | IO 抽象骨架，方向正确 | **保留为 L0 原型** |
| Session | `include/apollo/net/session.h:37-128` | 回调式：`onRecv(data,len)` 裸字节 + 返回处理字节数；send 直通 Connection；心跳仅 get/set 时间戳，无实现 | 接口形态不对（裸 TCP 语义漏给上层）；**重设计为 L2** |
| native_adapter | `include/apollo/net/adapters/native_adapter.h` | 自行开发 epoll 骨架 | 保留为 L0 唯一真实现 |
| sdnet_adapter | `include/apollo/net/adapters/sdnet_adapter.h:8,12-80` | **伪 SSCP**：`#define APOLLO_USE_SDNET` 后手写 `namespace SSCP`、ISSBase/ISSConnection/版本结构/错误码——仓库无任何配置链接真 SSEngine | **删除**（见 §6） |
| HTTP/WebSocket/RPC | `include/apollo/net/http/`、`websocket.h`、`rpc.h` | 外围能力雏形（出站 HTTP 现状证据见 §5.10） | 出站 HTTP 裁决见 §5.10（2026-09-30 补）；websocket 树归下轮审计候选（design-gap-inventory §4.2） |
| protocol 模块 | `modules/protocol`（codec/messages/nng_wrapper/socket） | NNG 封装 + 自行开发消息编码；**全仓库无 .proto 文件** | codec 并入 L1；nng_wrapper 见 §6 决策 |
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

隐藏清单（对游戏逻辑不可见）：连接建立与握手、加密、帧定界与 CRC 校验、心跳保活、seq/ack 与重连续传、水位与 trim、跨线程迁移（send 可从任意线程调，内部入 MPSC 环）、Lua 侧更只看到 `apollo.net` 的三件套（scripting-lua.md §8）。

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

### 4.3 上行侧：per-session 限流与风控计数（2026-09-30 补，design-gap-inventory #8）

§4.1/§4.2 全部是**下行**（服务器→客户端）的通道与水位；上行此前只有「intent only，服务端定夺」一句（sdk-contract §2.3 msg 注释）——恶意/故障客户端的持续上行灌包零防线（BW LoginConditions 类登录时点准入不管持续期，§7），本节补齐。上下行是镜像关系：下行防「发得出去」（预算+水位），上行防「收得进来」（限流+风控）。

- **判定位置**：IO 线程 L1 解帧后、投递 owning 场景线程**之前**——超限帧根本不进场景线程（单写者的 tick 预算不容上行洪水侵占）；计数器 per-session 且 IO 线程单线程读写，零锁。
- **参数表**（默认值，登录配置可覆盖；上限 = 预期稳态 ×3 量级——正常客户端永不触碰）：

| 通道 | 预期稳态 | msg/s 上限 | 字节上限 | 超限处置 |
|---|---|---|---|---|
| movement | 客户端 20Hz 上报 | 60 | 2KB/s | 丢旧帧（与下行「只发最新」同语义，位置以最新为准，无需告知） |
| attributes | ACK 捎带 ~10/s（客户端不发 SetAttribute） | 120 | 4KB/s | 丢弃 + violation_score |
| events | 技能/交互意图 ~5/s | 30 | 4KB/s | 丢弃 + control 通道回 throttle_notice（正常客户端的退避依据） |
| control | 心跳 1-2/s + ack 捎带 | 20 | 1KB/s | **硬超限直接断开**（control 通道不容灌——灌 control 是攻击不是噪声） |

- **总会话字节率**：8KB/s 稳态上限 + 64KB 突发桶（token bucket——与下行 attribute-sync §5.1 同型，方向相反）。
- **处置三级**：soft（单通道超限→丢弃+计数）/ hard（总字节超限→丢弃+throttle_notice）/ abuse（**violation_score** 连续 N tick 超阈→断开+风控记录，账号级，供 login-app 准入闸门消费）。
- **violation_score 单桶**：限流计数 + L1 schema 校验失败计数 + attribute-sync §9 权威纠正计数**同一风控桶**——「灌包」与「语义非法」在风控面同权（入口闸与语义闸共享出口）。

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
| term buffer 单写者环形分段 | 发送环/接收环的数据结构蓝本（自行开发 MPSC 环直接按此设计） |
| per-stream sessionId 隔离 | §4 四通道即 apollo 的流隔离粒度 |
| flow control 聚合窗口 | 多观察者下行聚合：慢观察者拉低整批发送速率（AOI 广播的 per-viewer 预算，attribute-sync §5） |
| IPC 共享内存传输 | 进程间同机通道候选（与 sdshmem 同框，ssengine-reference.md §4.4） |
| Archive 回放 | P3：跨服消息审计/故障重放（可选） |

### 5.3 结论：哪里用、哪里不用

| 路径 | 决策 | 理由 |
|---|---|---|
| 客户端 ↔ 服务器（海量 TCP/WS 长连接，万级 conn） | **自行开发薄封装**（本设计 L0–L3） | Aeron 不做 TCP 长连接接入；media driver 每连接开销、UDP 玩家侧不可靠网络适配、运维复杂度全不匹配。玩家路径的问题是"每连接会话语义"，这正是 Aeron 刻意不做的层 |
| 进程间总线（gateway↔game↔world↔db-proxy，机器间+同机） | **自行开发（已定，architecture-review §15.2）**，语义照 §5.2 抄（term buffer 单写者分段/流控聚合/NAK/offer 返回码） | nng 退役后禁止再引第二套进程间通信；自行开发蓝本现成——§5.2 吸收清单 + §5.5 Mercury filter 双族 + udp_channel 窗口重传 |
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

BigWorld 的 Mercury 是**演进了二十年的内部网络层**，不是一次性交付的库——filter 双族（§5.5）、udp_channel 窗口重传等能力是多年逐步叠加的（源证 architecture-review §16.7.1），游戏层 API 在这期间保持稳定。apollo 的自行开发决定（§15.2）要学的正是这个形态：**自行开发 = 拥有可持续优化的内核**，不是「一次写完对标二十年」——引外部库反而把优化节奏交给别人。apollo 的内部网络层 = 本设计的 L0-L3（客户端/单进程路径）+ 进程间路径，**同一套语义、同一个层**（§2 四动作、SendCode 返回码、四通道、水位机、FrameFilter 槽位两路通用），差异只在传输与拓扑。演进阶梯：

| 阶段 | 交付 | 性能口径 |
|---|---|---|
| **M0 语义定型**（P1，随本设计落地） | 四动作 API、SendCode 三返回码、四通道、水位机、FrameFilter 槽位——在客户端/单进程路径落地即定型；进程间路径消费**同一套语义**，不开第二 API 面 | 无（接口层，定型的是语义不是实现） |
| **M1 最小正确内核**（P3 首批） | 进程间段：帧定界 + contract_route 按 id 分发（sdk-contract §10.3）+ 背压水位 + 单播拓扑 + **连接设施 InterServerLink（§5.7——连接生命周期/重连/对上层开放）**；同机段 shm 形态同批定（§6 决策行——进不进首版看同机部署有无） | **正确性优先，不设性能指标**——初期内部流量小，过早优化无对象 |
| **M2+ 持续优化**（无截止，按需小批） | 窗口重传（udp_channel 蓝本）、流控聚合（Aeron 蓝本 §5.2）、filter 插件族扩充（加密/审计）、bundle 延迟聚合（一 tick 攒包一次冲刷——attribute-sync §7.1「每客户端每 tick 至多一帧」即其同型，推广到进程间）、同机 shm 通道实现（sdshmem 候选，G-2 镜像流——形态 M1 定、实现可后置） | **每项先有基准再动手**，独立小批、可回退 |

两条纪律保证「可持续」：

- **优化只发生在层内**：消费方（属性同步/脚本/业务）只依赖 §2 四动作与通道语义——内核重写、传输替换、filter 增删对它们零感知。Mercury 二十年演进不改游戏层 API 是同一事实；这是「游戏逻辑看不到网络」（§2 隐藏清单）的长期红利，也是它存在的目的。
- **每级有各自的验收对象**：M0 验收语义冻结（接口评审 + 契约入 sdk-contract）；M1 只验收正确性（分发不丢不重、背压可见、断连可检）；M2+ 每项优化以基准数据准入——同时防「为优化而优化」与「假装在演进」两种失败形态。

与 §7 分期的对应：原「P3 总线落地」口径改为「**M1 上线**」；M2+ 不占分期里程碑——它们是内部网络层的**常态工作**，不是路线图节点。

### 5.7 进程间连接设施（InterServerLink）：对上层开放的稳定连接（2026-09-29 增补）

§5.6 M1 把进程间段定型为「框架内部总线」；但**上层业务同样需要这层**——跨服玩法、匹配、聊天、GM/运维工具、collector 推送，要的都是同几件事：稳定连接、断连感知、自动重连、背压。本节把它定型为一等公民公开设施。BigWorld 的形态即如此：全部服务器代码直用 Mercury，entity mailbox、备份流、工具进程共一套网络库（cellappmgr.cpp 单文件 Bundle/Channel 命中 25 行）——「对上层开放」不是给 apollo 加的便利，而是这层在原版里的本来形态。两件 Mercury 新实证为直接参照：TCPConnectionOpener（出站连接状态机）与 LoggerEndpoint（上层消费者自建稳定连接的样例兼反面教材）。

**词汇：Link 一级，不再叠会话层**

- `InterServerLink` = 一条物理承载（TCP 先行；同机 P3 可换 shm——§6 sdshmem 行照旧）上的连接生命周期管理：拨号/接受、握手鉴权、心跳与断连检测、重连、水位。QoS 不在连接上再分层——四通道语义（§4.1）按**消息级标记**复用（reliable/unreliable/control 归消息分类声明），一条 Link 一组水位。
- 拨号/接受角色由 G-1 编队拓扑决定（配置态写死，不做运行期协商）；握手三元组 = **进程身份（G-1 组件 id）+ route_version + authority_epoch**——重连时 epoch 失配即判「对端已重启」，与断线重连是**两种事件**（前者要清状态重协商，后者只补传输）。

**连接器（机制层）：Mercury TCPConnectionOpener 蓝本**

- 形态：非阻塞 connect + POLLOUT 监听 + 一次性超时定时器（tcp_connection_opener.cpp:53/:99-101）；成功路径查 SO_ERROR 后把 endpoint 交给通道工厂并回调 `onTCPConnect(channel)`（:113-146）；失败按 errno 分类成类型化 Reason——ECONNREFUSED/ENETUNREACH → NO_SUCH_PORT、ETIMEDOUT → TIMER_EXPIRED（:186-231）。apollo 照抄此形态（失败分类与 SendCode 返回码同一哲学：类型化原因，不代掷猜测）。
- **连接器不自带重试**（Mercury 同款——失败即回调，重试是策略不是机制）：重试归 Link 管理器，指数退避 + **编队事件联动**——G-1 死亡通知到达即停止对该进程重拨（死了的进程不拨），复活通知重启退避。
- 反面教材是 BW 自己：LoggerEndpoint 的重连策略是消费者各写一份的硬编码（连续失败上限 3 次，logger_endpoint.cpp:672-703）——apollo 收敛为 Link 管理器统一一处，消费者只收事件不写策略。

**断连期间的 in-flight 语义（按 invoke_mode 分）**

| 消息模式 | 断连期间 | 重连后 |
|---|---|---|
| OneWay | 丢弃或排队，按消息 flags 声明（默认丢弃——跨服「尽力」语义） | — |
| ReliableEvent | 排队（**有界**：上限 + 水位） | seq 续传（§3 L2 resume 的进程间版）；对端重启（epoch 失配）→ 清队 + 上层裁决 |
| RequestReply | 挂起 future 照常走超时 | 重连不复活已超时请求（幂等性归 idempotent 字段——调用方声明） |

- 有界排队的先例与教训同源：BW LoggerEndpoint 的 send 在断连/拥塞时入有界缓冲，超 maxBufferedSize 即丢弃并报错——**「断连排队」与「背压」是同一机制的两个名字，都必须有界**（无界排队 = 把背压推迟成内存炸弹）。
- 三个事件回调对上层开放：`onLinkUp / onLinkDown(reason) / onPeerRestart(epoch_old→epoch_new)`——跨服业务在 onPeerRestart 里实现自己的会话恢复策略（重发查询/重协商），框架不代办。

**背压语义（服务器间与客户端的分界）**

- 四级水位（§4.2）照搬，但**服务器间不 trim**：客户端侧「低优先级可丢」是游戏体验权衡；内部消息每条的丢弃资格归**消息分类声明**——G-2 镜像流可声明丢旧帧（journal 位点兜底），RemoteEntityCall 默认不可丢。
- offer 返回码哲学不变：BACK_PRESSURED 时调用方决策（排队上限/降频/断言），框架不代掷。
- per-Link 水位独立：慢进程只拖累自己的 Link，不拖累别的进程对（Aeron per-image 隔离的进程间版，§5.2）。

**对上层开放的形态与边界**

- API 族 = §2 四动作的进程间实例（send/subscribe/state/close）+ 连接生命周期回调；**上层与框架内部消费同一设施**：跨服玩法/GM 工具/collector push（logging.md §5）与 RemoteEntityCall/route control/G-2 镜像流同源，无第二 API 面——M0「同一套语义、同一个层」在进程间域的兑现。
- 获取连接 = **编队拓扑内的具名端点**（G-1 服务发现）；不开放任意地址拨号——跨进程目标必须可被编队枚举（运维可审计，防误连生产环境外目标）。
- **不开放**：裸 socket（用户要的是稳定连接语义，不是 socket）；运行期字符串协议注册——上层自定义消息进契约 internal 域（sdk-contract §11，id 900+ 分段管辖），Mercury InterfaceMinder 单表病根（RemoteEntityCall 块已录）不学。

**Mercury 对照与取舍**

| Mercury | apollo 对应 | 取舍 |
|---|---|---|
| TCPConnectionOpener（连接状态机 + errno 失败分类） | Link 连接器 | **照抄机制**；重试留在外面（同 Mercury——机制与策略分离） |
| LoggerEndpoint 重连（每消费者自写、上限硬编码 :672-703） | Link 管理器统一退避 + 编队事件联动 | **不学散装**——策略收敛一处，消费者只收事件 |
| Channel TCP/UDP 双模 + 窗口重传 | Link + M2+ 窗口重传 | 窗口归 M2+（§5.6 演进阶梯） |
| Bundle 攒批发送 | M2+ bundle 延迟聚合 | 同型（§5.6 已列） |
| Interface/InterfaceMinder 命名方法表 | 契约 internal 域方法 id | **不学**（单表病根，§7 RemoteEntityCall 块） |
| filter 双族 | §5.5 FrameFilter | 照旧 |
| 服务器代码全员直用 Mercury | 上层与框架共用 InterServerLink | 本节定型点 |

### 5.8 L0 传输模型：就绪（reactor）与完成（proactor）两族——接口与运行模式（2026-09-29 补）

§3 分层图的 L0 行列了 epoll/io_uring/IOCP 槽位、§15.2 定 B 栈为唯一底座并写明演进路径——但「两族模型在**接口形状与运行模式**上差在哪、L0 接口如何同时承载两族」此前零分析（本节补）。不定形这层，将来换后端就是重写：IOCP 已成熟、io_uring 是 Linux 新方向，设计期必须把接口缝留对。

**两族分野（接口层）**

|  | Reactor（就绪通知） | Proactor（完成通知） |
|---|---|---|
| 系统调用族 | select/poll/epoll/kqueue | Windows IOCP、Linux io_uring（SQ/CQ） |
| 语义 | 「fd 可读/可写了，你自己动手」 | 「把缓冲交给我，完成了通知你」 |
| 回调签名 | `(fd, readable\|writable)` | `(bytes_transferred, error)` |
| 缓冲所有权 | 应用持有；非阻塞短读短写自己循环 | **内核/队列持有直到完成**——完成前不得复用 |
| 每次 IO 成本 | wait 批量摊销 + 每次读写一次 syscall | 提交/完成各一次进出队（io_uring 配 registered buffer 可零拷贝） |
| 背压 | 不内置（自己叠水位） | **投递深度天然是背压**（SQE 深度 / 未完成 IO 数） |
| 取消 | 无此问题（同步 read/write） | 必须定义（close 时在途操作：io_uring ASYNC_CANCEL / IOCP CancelIoEx） |

io_uring 是两族超集（`IORING_OP_POLL_ADD` 可当纯就绪用、带缓冲读写走完成路径、provided buffer ring 收方向零拷贝）——但**收益只在完成路径上**：只拿它模拟 reactor 等于白引。

**三框架先例：全是 reactor（本轮实读）**

- BW Mercury：`EventPoller` 抽象基类（event_poller.hpp:95-137，doRegisterForRead/Write + processPendingEvents 纯虚）+ **仅 select 实现**（event_dispatcher.cpp:370 注释自述 select；lib/network 无第二 poller 文件）。
- KBEngine：poller 抽象 + epoll/select 双后端（poller_epoll.cpp 140 行 / poller_select.cpp 256 行）。
- skynet：socket 线程 SP_* 宏抽象（socket_epoll.h / socket_kqueue.h，:20 `epoll_create`）。
- 结论：**MMO 主流二十年停在就绪家族**——瓶颈从来不在 syscall 密度而在每连接会话语义（seq/ack/水位/重连即 L1/L2，§1 核心判断）；完成模型是 Aeron/Seastar 类高吞吐消息层的战场。apollo 以 reactor 为基线不是落后，是与全部先例同型；完成模型列 M2+ 演进项（下文）。

**apollo 仓现状：三处 L0 尝试（本轮实读；含 C-29 盘点边界外一处）**

| 处 | 位置 | 模型 | 形态与问题 |
|---|---|---|---|
| B 栈（唯一进默认构建，§15.2 底座） | include/apollo/network/transport/reactor.hpp + modules/net/tcp/src/reactor.cpp | poll(2) reactor | AddSocket/ModifySocket/RemoveSocket + EventCallback(fd, events) + EventLoop 独立线程（reactor 接口形状正确）；reactor.cpp:57-59 POLLIN/POLLOUT/pollfd 类型别名缝、:92 `poll(fds, n, 1000)` 轮询等待；socketMutex_ 全表锁；Windows 分支缺（`#ifndef _WIN32` include poll.h） |
| 栈 A | modules/net/rpc/src/adapters/native_adapter.cpp（1109 行） | IOCP + epoll 双后端骨架 | initialize 双宏分支（:778 CreateIoCompletionPort / :786 epoll_create1）+ PostQueuedCompletionStatus 退出通知；**但 ioThreadProc 实为全连接轮询**（:1027-1075 每 100ms 遍历 listeners/connectors/connections 全表调 handleRecv/handleSend——epoll fd 建了而事件循环没消费它，C-18 FileWatcher「死 inotify」同族）；IO 线程池 hardware_concurrency() 默认 4 |
| ipc 树（默认 OFF，§15.2 已注下轮审计） | include/apollo/ipc/async_io.h（572 行） | **完成模型接口**（仓内唯一） | `IoMultiplexer`：postRead/postWrite/postAccept/postConnect + IoEvent{result, error, op} 完成回调 + runOnce；三后端探测宏（:12 IOCP / :16-18 `__has_include(<linux/io_uring.h>)`→URING / :21-25 EPOLL 兜底 / KQUEUE）；**AsyncOp 携 `std::vector<uint8_t> buffer` 值拷贝**——完成模型接口却用拷贝语义，零拷贝与背压物理基础都没做 |

（栈 A「epoll 建而不用」与 ipc 树接口是 C-29 四栈盘点边界外的增量事实——ipc 树本就是登记在案的下轮审计对象；均记入 architecture-review 登记簿。）

**L0 接口定形：发送环提交语义（两族通吃的关键）**

- §6 现行收敛口径「事件源 + writev + 水位」是 **reactor 形状**。照此定接口，IOCP/io_uring 只能退化使用（每完成一次重新等可写再写——IOCP 上的经典反模式）。
- 正确定形：L0 对上暴露**环提交**而非 socket：`post_send(环切片) / on_send_complete(n) / post_recv(缓冲) / on_recv(缓冲, n)`。reactor 适配器内部把它翻译成「epoll 就绪 → 从环取数据 writev → 同步返回即完成回调」；completion 适配器把切片直接投 SQE/OVERLAPPED。L2/线程模型（§3：场景线程单写者入环、IO 线程唯一消费者 flush）在两族下同一形状——**环即投递单元**，§5.2 term buffer 蓝本的深化。
- **缓冲所有权规则**：完成模型下发送切片在完成前不得复用 → 环容量 = 最大在途字节上限 → §4.2 水位机判据在两族下同为「环占用」，零改动——这是背压建在环上而非 socket 上的直接红利。ipc 树 AsyncOp 的 vector 拷贝正是没想清这一步的接口形态（拷贝换来安全，丢掉零拷贝、背压物理基础与水位语义）——**不学**。
- **取消语义统一口径**：close() 时在途操作照常完成但回调带 ABORTED（io_uring IORING_OP_ASYNC_CANCEL / IOCP CancelIoEx + ERROR_OPERATION_ABORTED 映射到同一 Reason），资源归还环，连接状态机不等在途——与 §5.7 断连 in-flight 语义表衔接。

**运行模式（各后端）**

- **epoll（M1 基线）**：poll→epoll 是 M1 内第一刀（B 栈 reactor.cpp:92 的 1000ms poll 轮询即替换对象）；每 IO 线程一个 epoll 实例（多线程共享单 fd 的 events 分发需归一，栈 A 现状要改）；level-trigger + 批量 writev 聚合；tick 交界 flush（§3 线程模型不变）。
- **io_uring（M2+，按需）**：**每线程一个 ring**（SQ/CQ 无锁的前提是 ring 私有）；**不开 SQPOLL**（内核轮询线程与游戏服抢核，弊大于利）；每轮 submit+reap 批量；provided buffer ring 可选（收方向零拷贝）。**基准准入**（§5.6 纪律）：先以真实连接数/包型测 syscall 数与尾延迟对比 epoll，赢了才切——切换本身是层内替换（本节接口定形保证）。
- **IOCP（不进路线图）**：服务器 = Linux 唯一部署目标；Windows 仅开发机——栈 A/native_adapter 的 `_WIN32` 分支保留为**可编译性垫片**（防开发机构建腐烂），不作性能承诺、不投入优化。同理 kqueue 不投入（macOS 开发机）。

**与 §5.6 演进阶梯的衔接**：M1 = epoll（含 poll→epoll 首刀）；M2+ = io_uring 按需（基准准入）；IOCP/kqueue = 非目标（垫片级）。接口按本节「环提交语义」定形后，全部替换是层内替换、消费方零感知——§5.6「优化只发生在层内」纪律在 L0 的兑现点。

### 5.9 客户端通道安全与加密库选型（2026-09-30 补，design-gap-inventory #4）

sdk-contract §10.5 帧层图的 `[加密 P3]` 占位在此定型：加密 = **FrameFilter 族的 CryptoFilter**（§5.5 槽位；filter 位次已定 sdk-contract §10.5——**压缩之后**：密文不可压缩，先压后密），握手归 control 通道，密钥不出 filter 层。

- **威胁模型（先裁敌人再选武器）**——防：窃听（公共网络明文嗅探）/ 重放（截帧重发）/ 重连劫持（盗 resume token 冒充续连）/ 中间人（握手篡改）；**非目标：客户端逆向与作弊**——客户端二进制可逆，任何客户端侧密钥皆可提取；防作弊靠服务端权威判定（attribute-sync §9）+ 复算 hash 链（battle-determinism §5），不靠通道加密。加密防「网络上的第三方」，不防「持客户端的玩家」——此声明拦截「加密=反作弊」的常见错位投入。

**握手序列（TCP 主路径；鉴权与密钥协商一次完成）**：

```
C → S  ClientHello { login_token, nonce_c, pub_c(X25519), aead[] }   // control 通道
S → C  ServerHello { nonce_s, pub_s(X25519), aead_selected }
双方    shared = X25519(own_priv, peer_pub)
        session_key = HKDF-SHA256(shared, salt = nonce_c‖nonce_s,
                                  info = "apollo-session-v1"‖session_id)
此后    全帧 CryptoFilter：AEAD(session_key, nonce96 = nonce_c_hi64 ‖ seq32)
```

- **login_token 归 login-app 签发**（账号鉴权与游戏会话职责分离：token 短 TTL + 一次性，游戏进程只验签不触账号库；KBE loginapp 同型分工）。
- **per-frame nonce 由 seq 派生**（nonce96 = 握手 nonce 高 64 位 ‖ 帧 seq 低 32 位）：seq 单调（§3 L1）保证同 key 下 nonce 永不重用——GCM nonce 重用是灾难性失守，seq 与加密在这一点共生。
- **防重放**：AEAD 解密前查接收滑动窗口（bitmap，L2 SACK 同思路）——窗口外/重复 seq 丢弃 + violation_score（§4.3 同桶）。
- **前向保密**：每连接（含 resume 重连）重走 ECDH——resume 恢复 L2 seq/队列状态，**不恢复旧密钥**；token 一次性（用后作废）+ 握手 nonce 新鲜性拦截劫持者的握手竞争。
- **密钥层次三层**：login_token（账号域，login-app）/ session_key（连接域，每连接新协商）/ per-frame nonce（帧域，seq 派生）——HKDF 单向派生，不回溯。

**算法分级表**（进契约文档与评审红线）：

| 用途 | 选型 | 备注 |
|---|---|---|
| 密钥协商 | X25519 | 恒定时间实现成熟 |
| 对称加密 | AES-128-GCM（首选，AES-NI）/ ChaCha20-Poly1305（备选，无 AES-NI 客户端） | AEAD 统一形态 |
| 密钥派生 | HKDF-SHA256 | |
| 摘要 | SHA-256 | SHA-1 仅限 WS 握手存量协议（RFC 6455 既有语义） |
| **禁** | MD5、SHA-1（新代码）、RC4、DES/3DES、裸 ECB、无 MAC 的 CBC | 库仍提供 ≠ 可用 |

**加密库：OpenSSL 单一 crypto 源（定案）**——vcpkg.json:6 已直依赖；websocket.cpp:38 SHA-1（WS 握手）与 modules/net/CMakeLists.txt:135-160 built-in 分支 `find_package(OpenSSL)` 即既有消费面。**不引第二套**（libsodium/Botan 全仓零命中，保持）——「禁并存」纪律（§15.2）的密码学版：双 crypto 库 = 双 CVE 面 + 算法行为分裂。用法纪律：只走 EVP 高级接口（不碰底层原语）、tag/密文比较用 `CRYPTO_memcmp`（恒定时间）、随机数只用 `RAND_bytes`（禁自造熵源）；版本锁 OpenSSL 3.x LTS。

- **WSS 兜底形态**：WS 页端客户端的加密由 wss（TLS）承载，与 TCP+CryptoFilter 双形态并存——P3 随 gateway-app 定案；TCP 自定义握手是主路径（帧开销可控 + filter 栈统一），wss 只为浏览器端兜底。

### 5.10 出站 HTTP 与第三方接出（2026-09-30 补，design-gap-inventory #11）

服务器**出站** HTTP（第三方登录验证/支付回调/推送）是 P3 真需求；现状是「5048 行已写、默认构建全桩」——modules/net/http（rest_client.h 343 + rest_client.cpp 734 + http.cpp 816 + event_loop.cpp 648 + websocket.cpp 1193 + 三公共头 1314，登记时漏计后两项——architecture-review §18 勘误），rest_client.cpp:13 `#ifdef APOLLO_HAS_CURL`、:16 `APOLLO_CURL_STUB 1`（vcpkg.json 无 curl——与 C-45 宏门 MySQL 同族）；零生产消费方（仅 examples/tests/docs）。§1 现状表「另行处理」——本节即裁决。

**三项裁决**：

1. **依赖收口：curl 进 vcpkg，rest_client 转真实现**——libcurl 是事实标准（chunked/redirect/TLS/代理/HTTP2 全套自持），自写 HTTP 客户端是负资产；curl 的 TLS 后端配 OpenSSL（§5.9 单一 crypto 源不破）。`APOLLO_CURL_STUB` 桩路径删除随代码批执行（冻结面），方向此处定死。
2. **Drogon 备选分支删除**（CMakeLists.txt:81-91 http、:124-133 websocket 双处）——§15.2「禁止并存」的直接对象：web 框架与自有客户端双形态并存，两处都是「第五套网络栈」的种子。admin exporter 不需要 Drogon（logging.md §5.1：Prometheus 文本是几十行薄层）。
3. **线程模型：同步 API 禁场景线程直调**——RequestOptions 默认 timeoutMs=30000 的同步语义与 SQL/Redis 同族：一律走 scripting-lua §8 异步交接（执行层**按新建计** = IO 线程上的 curl_multi 多路复用——第十轮审计（architecture-review §18 C-57）实测既有 event_loop.cpp 为 poll(2) 单线程 reactor、与 curl 零关系零接线，**不构成「既有」执行层**，此处原措辞已勘误；完成回调带 request_id 回场景线程 tick 边界）；回包不进当 tick 判定（battle-determinism §1「异步不进判定」——落点为未来 tick 输入）。

**边界**：出站方向与客户端四通道相反，不共通道不共会话；**目标白名单**（域名/网段进配置、运行期不可加）——与 §5.7「不开放任意地址拨号」同纪律（SSRF 面：脚本可发起的出站目标必须可枚举）。modules/net/{http,websocket} 代码面审计（桩质量/测试资产处置）归下轮审计候选（登记簿已录）；本节只裁设计面。

## 6. 决策清单（保留/删除/引入）

| 对象 | 决策 | 依据 |
|---|---|---|
| Connection/Listener/EventLoop | **保留**为 L0 原型，接口收敛为"事件源 + writev + 水位" | 方向正确，缺的是上两层 |
| `sdnet_adapter.h` | **删除** | 伪 SSCP（sdnet_adapter.h:12-80），从未链接真库，空壳代码；精华已提炼为设计点（§5.4） |
| `session.h` 现接口 | **重设计**（不保留回调形态） | onRecv 裸字节把 TCP 语义漏给游戏逻辑，是本设计要消灭的头号泄漏点 |
| `modules/protocol` codec/messages | **并入 L1**，codec 迁到 protobuf（与 attribute-sync §7、sdk-contract 同一契约源） | 全仓库无 .proto，手写编码无法支撑多端 SDK |
| `nng_wrapper` | **已定删**（architecture-review §15.2 退役清单五项之一） | 两套进程间通信并存即重复；决策已闭环 |
| 四级水位/四通道/resume | **新建**（L2） | apollo 完全没有；属性同步设计的直接依赖 |
| Aeron | **已定不引入**；§5.2 语义吸收清单与 §5.5 filter 蓝本保留 | §5.3 已关闭为自行开发（architecture-review §15.2） |
| FrameFilter 体系 | **新建**于收敛后的 modules/net（四套收敛为前置条件） | architecture-review §16.8.3-①：BW 同库 + 注入式装配先例（udp_channel.hpp:81/:139-140） |
| L0 传输模型 | **reactor（epoll）为基线**；接口按「发送环提交语义」定形以兼容完成模型——io_uring M2+ 基准准入、IOCP/kqueue 不进路线图（垫片级） | §5.8（2026-09-29）：三框架先例全 reactor（BW event_poller.hpp:95-137 select-only、KBE poller_epoll.cpp、skynet socket_epoll.h:20）；ipc 树 async_io.h 为仓内既有完成模型接口（C-29 盘点外，随 ipc 审计轮处置） |
| sdshmem 类同机共享内存通道 | **暂缓**（P3 随进程间总线同批定案；引设计不引代码——与 Aeron 同等处理） | 只承载**同机大块只读共享与点对点镜像流**（空间格子快照/监控指标/G-2 热备镜像流），**不做通用消息总线**（architecture-review §15.2 禁第二套 IPC 并存的边界照旧）；ssengine-reference §4.4 + Aeron IPC 同框（§5.2） |
| 客户端通道加密 | **OpenSSL 单一 crypto 源 + CryptoFilter**（§5.9）：X25519 + AEAD + HKDF-SHA256；MD5/SHA-1（新代码）等禁用表进评审红线 | design-gap-inventory #4；libsodium 零命中保持；sdk-contract §10.5 占位收口 |
| 出站 HTTP | **curl 进 vcpkg + Drogon 分支删除 + 同步 API 禁场景线程直调 + 目标白名单**（§5.10） | design-gap-inventory #11；§15.2 禁并存纪律对象；SSRF 面 |

## 7. 分期落地

- **P1**：L1 帧格式 + L2 会话基础（seq/ack/心跳/四级水位）+ native adapter 收敛为唯一 L0；GameConnection Facade 上线，属性同步 P1 依赖本层 attributes 通道。
- **P2**：重连续传（resume token）+ movement 通道"只发最新" + 与属性预算的 hard 水位联动降档。
- **P3**：内部网络层 **M1 最小正确内核**上线（§5.6 演进阶梯——进程间段：帧定界 + contract_route 按 id 分发 + 背压水位 + 单播拓扑，正确性优先、不设性能指标；蓝本：§5.2 语义 + §5.5 Mercury filter 双族；同机段加评估 sdshmem 类共享内存通道——只承载大块只读共享与点对点镜像流（快照/指标/热备镜像），不做通用总线，§15.2 边界照旧，随内核同批定案，引设计不引代码：ssengine-reference §4.4、Aeron IPC 传输同框；窗口重传/流控聚合/bundle 聚合归 **M2+ 持续优化**，不占本里程碑）、网关模式（gateway-app 接入）、Archive 类消息审计；运维观测通道两截落位（architecture-review §16.8.3-⑤）：检测原语（per-scene 心跳版本号/队列水位/实体计数/帧耗时）内嵌 owning 模块并经 control 通道上行，聚合工具归 apps/（依赖面 = modules/runtime 的 ConsoleEvent/IConsoleEventSource，application_host.hpp:20/:28；先例 skynet debug_console/monitor、BW server/tools/{bw_profile,message_logger}）——模块零依赖 apps，ops 工具只触只读自省接口。**接出形态定案**（2026-09-30，design-gap-inventory #10）：游戏进程不直连 Kafka、不引 OTel SDK、不开 per-process HTTP 端口——单一 exporter（admin 吐 /metrics）+ 日志 tail 外采 + trace 子集，细则见 logging.md §5.1。

### P3 前置设计：进程编队与服务发现 / 备份容灾 / 实体远程调用（2026-09-29 补，architecture-review §16.4 G-1/G-2 + RPC 接线）

P1-P2 单进程阶段本层零落地；此节先把 P3 的前置形态定下来，防止到时拍脑袋（G-1/G-2 为原六份设计文档的空白，登记于 architecture-review §16.4；实体远程调用为既有 architecture/ 代设计的接线，同日补）。

**现状声明（显式）**：apollo 单进程阶段**无进程级高可用**——崩溃安全 = write-behind journal 数据不丢（attribute-sync §8.2）+ 重启拉起；backup/接管/迁移为零起步。这是分期不是遗漏。

**G-1 进程编队与服务发现**（进程如何被发现、如何感知彼此死亡）：

- 先例一 BigWorld：每机一个 bwmachined 守护进程，birth/death 通知经 machine_guard 协议订阅/广播（machine_guard.hpp:496-497/:609-613/:882-883）；cellappmgr/baseappmgr/dbappmgr 消费生死事件做编队决策，cellappmgr 兼负载平衡（cellappmgr.hpp:43/:192-194）。
- 先例二 KBEngine：无守护进程，machine 以 UDP 广播应答发现（machine.cpp:646-670，KBE_PORT_BROADCAST_DISCOVERY）——更轻，但「机器死了谁来报」无解。
- apollo 形态（P3 定稿口径）：两层并存——单机 machined 式守护（本机进程生死/拉起/崩溃上报）+ UDP 广播发现（跨机拓扑发现），分别取两先例长处；与 architecture-review §15.2 自行开发纪律对齐，不引 etcd/consul 类外部协调服务——游戏服进程拓扑小、变更低频，守护+广播的最终一致够用，外部强一致依赖换不来对等收益（**2026-09-30 用户裁决落档：不引入额外注册中心——原 docs/05 §2.3 注册中心稿（Redis/etcd+心跳 5s/30s）已整档删除，36 号 #13 行同步改写，git 可溯**）。控制面复用 §4.1 control 通道的进程间延伸：编队事件（进程加入/退出/机器死亡）作为 control 事件进各进程轮询源——不开新通道体系。

**G-2 备份与宕机接管**（BigWorld 全套先例；apollo P3 骨架取两件）：

- 先例件：baseapp 热备分帧发送（backup_sender.hpp:52-61）+ 一致性哈希备份链（backup_hash/backup_hash_chain——备机按链持续追主机实体状态流）+ reviver 协调接管（主机死亡后备机把镜像实体升级为权威；cellapp 死则 cellappmgr 在幸存 CellApp 重建 cell）+ secondary db 任务族 + dbappmgr 扩缩容哈希再分布（dbappmgr.cpp:472/:637）。KBEngine 对照：无此层（architecture-review C-51/§16.4）——宕机靠实体最后一次归档，窗口 = 归档周期。
- apollo P3 最小骨架：**backup-hash 链 + reviver 两件先行**；secondary db/动态扩缩容推迟到多 cell 稳定运行后。
- **备份粒度对齐单写者纪律**：热备流 = PersistJournal 的只读镜像消费（attribute-sync §8.2 journal 的第二消费者），不另起一套备份协议——备机 ack 的 journal 位点即接管起点，与属性 seq 语义（ViewerState.acked_seq 同模型）天然衔接；接管 = 备机在 journal 位点重放后于 tick 边界切换为权威写者（复用 attribute-sync §10.2 停机序列的镜像路径：先停旧主的写入认定，再切权）。镜像流传输形态 P3 定：**同机部署候选 = sdshmem 类 shm SPSC 环**（主机单写 journal 追加、备机单读消费——正是 §6 决策表「点对点镜像流」的典型场景；ack 仍走 control 通道），跨机则随进程间总线。

**负载均衡与异常恢复（manager 域，2026-09-29 补）**：

- **BW 先例（全套，本轮补证）**：
  - 负载即观测：cellapp 每 tick 三分负载统计上报（cellapp.cpp:1177-1190，§16.2 已引）；baseappmgr `minAppLoad()` 全遍历取最轻（baseappmgr.cpp:588-599）——新 base 落点 = 最轻 baseapp；过载触发登录准入闸门（:947-954，LoginConditions）并把 (addr, load) 对上报 loginapp 做登录分流（:1117）。
  - cell 侧再平衡：周期 `loadBalanceTimer_`/`overloadCheckTimer_`（cellappmgr.hpp:305/:307）+ `metaLoadBalance/loadBalance/updateRanges`（:233-236）+ cellapp 主动 `shouldOffload` 上行（:98）。
  - **异常恢复是排他相位**：mgr 向 machined 注册死亡监听（cellappmgr.cpp:276-277 `MachineDaemon::registerDeathListener → handleCellAppDeath`——G-1 编队事件的消费端）；死亡/自身重启即 `startRecovery()`（:281/:1411，cellappmgr.hpp:230-231），**恢复期间拒绝新请求**（:1300 "Denying %s since in middle recovery"）——防恢复中拓扑再变。
  - baseapp 接管：reviver **独立进程**（server/reviver/——component_reviver/reviver/reviver_config 三层）。
- **apollo 设计（P3 最小骨架——取 BW 骨架，不取全家桶）**：
  - **指标同源**：负载 = G-5 检测原语的聚合（帧耗时/实体数/队列水位，经 control 通道上报）——观测与调度消费**同一份数据**，不做第二套负载统计。
  - **分配与准入**：新场景/新会话落点 = 最轻进程（minAppLoad 同型 + 水位硬约束）；过载 = 准入闸门（拒登/排队，BW LoginConditions 同型）——负载既是调度输入也是准入依据。
  - **再平衡分层**：P3 只做「新负载往轻处走」+ 过载告警 + 手动搬迁工具；**自动 cell 迁移（BW meta balancing/shouldOffload 族）推迟 M2+**——迁移协议依赖实体搬迁与视图重建全套，不属于最小骨架。
  - **异常恢复链（按死者角色分）**：检测 = machined 死亡事件（G-1）→ mgr 进入**恢复相位（排他——期间拒绝新场景/新进程加入，BW :1300 先例）**；base 死 = reviver 式接管（G-2 backup-hash 链 + journal 位点重放切权）；cell 死 = mgr 在幸存进程重建场景（权威源 = write-behind journal attribute-sync §8.2 + base 侧重放）；进程本身拉起归 machined（重启策略 = 编队配置，mgr 只消费事件不做进程管理）。
  - **两层恢复独立成立**：进程级恢复（本节）与连接级恢复（§5.7 onLinkDown/自动重连）互不依赖——客户端重连走 §3 resume，进程重建走本节，谁先完成谁先服务。
  - **会话与在线目录**（谁在线/在哪条线/顶号裁决/掉线保活窗口/RouteResolver 宿主定位供数）= manager 域的另一职责件——进程内存权威态（「全局仲裁态集中不共享」+「会话」两行通道的落地）、事件上报 + 周期对账、崩溃恢复 = 全量重报重建（同本节恢复相位）；全量设计见 `docs/design/session-and-online-directory.md`（design-gap-inventory #12，2026-09-30 落盘）。
  - 「是否参考 BigWorld」：**是，且只能参考它**——三家仅 BW 有完整此层（KBE 无进程级容灾，architecture-review C-51/§16.4；skynet 单节点无编队）；明确不搬：动态 cell 迁移、secondary db、自动扩缩容（M2+ 按需评估）。

**进程间信息共享与同步模型（2026-09-29 补）**：

- 原则一句话：**每条信息有唯一 owner 进程，「共享」= 订阅 owner 的投影**——单写者纪律的进程间延伸。不存在「两个进程可写同一块内存」的通用共享（跨进程锁把死锁域扩大到 OS 级，且破坏所有权模型；§15.2「禁第二套 IPC」边界照旧）。
- **四类通道**：

| 信息形态 | 通道 | 同步机制 |
|---|---|---|
| 有 ownership 的动态状态（实体属性/会话/场景） | InterServerLink 消息（§5.7） | owner 广播 delta，消费者本地镜像 + seq/位点续传 |
| 读多写少的远程镜像（ghost 视图 / G-2 热备） | RO_MIRROR 镜像流（attribute-sync §4.4 / 本节 G-2 backup-hash） | journal 位点 ack + authority_epoch 防回写 |
| 同机只读大块（空间快照/监控指标/加载后的配置表） | sdshmem 类 shm（§5.3/§6 决策行——只承载大块只读，不做通用总线） | **换页发布**：新版本写新页 + 原子切指针，读侧永见完整版本（SSEngine sdshmem 先例，ssengine-reference §4.4） |
| 全局仲裁态（编队拓扑/持久数据/日志） | **集中不共享**：machined+mgr（G-1）/ DB journal（attribute-sync §8.2）/ collector（logging.md） | 事件广播 + 落盘 |

- **「同步」三层含义分清（防混用）**：① **传输同步**（seq/ack/续传）——连接层（§3/§5.7）的事；② **状态同步**（镜像位点/epoch）——owner-订阅模型的事（attribute-sync §3 ViewerState 推广到进程间）；③ **数据一致**（journal/落库）——持久层的事（attribute-sync §8.2）。三层各自独立成立、互不兜底——「消息到了」≠「镜像追平」≠「落库了」，每层各有自己的恢复路径（续传 / 快照重置 / journal 重放）。

**实体远程调用（RemoteEntityCall——BW EntityMailbox / KBE EntityCall 的对应物）**：

- **语义层设计已存在，本设计只接底座**：`docs/architecture/remote-entity-call-design.md`（architecture/ 代）四件套——`RemoteEntityRef`（entity_id/entity_type/target_domain/authority_role/route_version/shard_key）、`RemoteMethodSchema`（method_alias/invoke_mode/arg_types/timeout_ms/idempotent）、`InternalMessageEnvelope`（trace_id/request_id/source/target_app/entity_id/method_alias/route_version/authority_epoch）、`RouteResolver`（宿主定位/route 版本校验/ghost 转发）+ 消息四分类（EntityMethodCall/RouteControl/LifecycleEvent/ReplicationCommand）——语义层以该文档为准，此处做三件接线与对齐：
- **传输底座 = §5.6 M1 内核**（按 id 分发单播；连接级语义 = §5.7 InterServerLink——断连/重连/对端重启事件，invoke_mode 的断连处置即其 in-flight 语义表）：该文档所引 `Channel/Endpoint`（modules/net/protocol 旧形态）一律按本设计 L0-L3 口径读作 M1 内核接口；envelope 走 internal 域消息，不另起协议。
- **术语对齐**（该文档写于 design 语料定稿前）：`target_domain` 的 Ghost ≈ attribute-sync §4.4 `RO_MIRROR`（远程只读镜像，调用转发权威侧）；World ≈ CELL 权威侧；Anchor/Proxy ≈ base 侧（登录/会话入口）——两套词汇指同一权威模型，落地统一为 attribute-sync §2.3 双轴标记（所有权轴 × 可见域轴）。
- **invoke_mode 判定（对照两家刻意不做的事）**：BW/KBE 的实体调用均为**异步单向、无返回值**（结果用反向调用）——同步返回把网络 RTT 引进 tick，与 G-7 的确定性节拍论证冲突。apollo 口径：**OneWay 为默认**；RequestReply（request_id + future）只限低频控制面（跨进程 DB/GM/运维），永不进热路径；ReliableEvent 复用 events 通道语义。
- **信封与契约合流**：`InternalMessageEnvelope` = sdk-contract §11 internal 域消息（id 900+）的统一信封——request_id/trace_id/route_version/authority_epoch 为信封标准字段，进契约由生成器产出（contract_route 清单扩展：method alias → internal 消息绑定），不手写第二份。
- **先例病根规避**：方法 id 若走两家同病的单一分配表（§11.1 KBE message_handlers 单表、§11.2 BW InterfaceMinder 单表——内部演进推动客户端重发版），即重蹈覆辙；apollo 的方法 id 归契约 internal 域分段管辖，域分段四规则（sdk-contract §11.3）天然免疫。

## 8. 与其余设计的交集

| 关联设计 | 落点 |
|---|---|
| attribute-sync.md | 四通道即其 §5.3 QoS 分割；水位 hard 档触发其 token bucket 降档；resume 依赖其 ViewerState.acked_seq 模型；帧压缩同 §7（zstd>256B） |
| scripting-lua.md | 脚本只见 send/subscribe/回调三件套；异步回调按 tick 边界 resume 协程；背压/重连对脚本不可见 |
| sdk-contract.md | 契约文件是 L1 编码与各端 SDK 的同一来源；帧格式写入契约（各端插件据此实现帧定界） |
| ssengine-reference.md | DelaySend/GetSendBufFree/GATE/sdpkg 四点收编（§5.4）；sdnet_adapter 反例（伪命名空间）与 fake-fruit 同类 |
| architecture-review.md | 网络适配器注册走 core::di 启动期装配（编译期类型键），不进旧字符串容器；adapters 的存在形态=链接期选择，非运行期字符串切换 |
| xml-generation.md | 帧头 ver → filter 栈声明（16.7.1）由契约生成器装配（用途④）；messages.xml 的通道 enumeration 由其四层漏斗第①层校验 |
| logging.md | collector push = §5.7 InterServerLink 的上层消费者（进程间稳定连接，独立于客户端会话四通道）；Link 有界排队与「collector 挂 → 只写本地」降级语义同源（BW LoggerEndpoint 有界重连 + 有界缓冲先例） |
| session-and-online-directory.md | §7 共享模型四类通道表「会话」行与「全局仲裁态」行的落地件——在线目录 = manager 域集中权威 + InterServerLink 事件投影（RouteResolver 镜像供数）；§3 resume TTL = 掉线保活窗口同源值；§5.7 authority_epoch = 顶号竞争裁决兜底（anchor_epoch）；§5.9 login_token = 目录登记的入场前置 |
| login-flow.md | §5.9 login_token 归属行（:313）与握手族的全链兑现——两阶段连接（登录连接匿名握手 info="apollo-login-v1" 域分离 + 游戏连接 ClientHello 带 token）；§5.9 :317 密钥三层的凭证面补全（三凭证辨析 concept-glossary §2.5）；§5.7 Admission RPC = RequestReply 低频控制面实例；§7 闸门/最轻分配/顶号预裁 = 登录准入处理序 |
| inbound-interfaces.md | §5.10 出站三裁决的**镜像面**（curl 出站复用/渠道域进出站白名单/Drogon 删除裁决维持不复议）；§5.7 InterServerLink = 回调第二段投递通道（OneWay + per-Link 背压 + 具名端点）；§7 G-1 编队声明 interfaces 组件——独立进程承载入站 HTTP（缺口 #14，2026-10-01 落盘） |

---

*基线：apollo main @ 35a9c528（include/apollo/net、modules/protocol 读码）；Aeron 参考其官方仓库文档与 C++ 客户端源码概念；SSEngine sdnet 读码对照。2026-09-28 同步修订（②）：Aeron/nng 决策关闭（architecture-review §15.2）、§5.5 Mercury filter 双族蓝本增补（§16.7.1 源证）。2026-09-29 增补（③）：filter 归属/前置条件与决策表 FrameFilter 行（architecture-review §16.9.2 粘贴）、§7 P3 观测通道行（§16.9.5 粘贴）、新增「P3 前置设计」节（G-1/G-2，architecture-review §16.4 空白的补设计）。同日增补（④）：sdshmem 同机共享内存由「暂缓观察」升格为 P3 正式候选（与 Aeron 同等处理——引设计不引代码、随自行开发总线同批定案）——§5.3 行改口径、§6 新增决策行、§7 P3 总线段加评估、G-2 热备镜像流补同机传输候选（ssengine-reference §4.4 既有登记的接线）。同日增补（⑤）：新增 §5.6「内部网络层的演进策略」——自行开发 = 拥有可持续优化的内核（Mercury 二十年演进同型）：M0 语义定型（P1）/ M1 最小正确内核（P3，正确性优先不设性能指标）/ M2+ 持续优化（无截止、每项先有基准）；摘要 7 与 §7 P3 里程碑口径同步（「总线一次性落地」→「M1 上线」，窗口重传/流控聚合/bundle 聚合移出路线图归常态优化）。同日增补（⑥）：§7 P3 前置设计补实体远程调用块（RemoteEntityCall——语义层引用 architecture/remote-entity-call-design.md 四件套，传输底座接 §5.6 M1，invoke_mode 定 OneWay 默认/RequestReply 只限控制面，信封合流 sdk-contract internal 域）。同日增补（⑦）：新增 §5.7「进程间连接设施（InterServerLink）」——对上层开放的稳定连接语义（连接器/统一重连/in-flight 分类/背压分界/公开面边界）；Mercury 两件新实证：TCPConnectionOpener（tcp_connection_opener.cpp:53/:99-101/:113-146/:186-231 全文实读）与 LoggerEndpoint（logger_endpoint.cpp:672-703 有界重连 + send 有界缓冲）；摘要 8、§5.6 M1 行、§7 传输底座行、§8 交集表（logging.md 行）联动。同日增补（⑧）：§7 P3 前置设计补两块——「负载均衡与异常恢复（manager 域）」（BW 补证：baseappmgr.cpp:588-599 最轻分配/:947-954 过载准入/:1117 负载分流、cellappmgr.hpp:98/:233-236/:305/:307 再平衡族、cellappmgr.cpp:276-277 machined 死亡监听/:281/:1411 startRecovery/:1300 恢复期拒新请求、server/reviver/ 独立进程；apollo 取骨架不取全家桶——自动 cell 迁移推迟 M2+）与「进程间信息共享与同步模型」（owner-订阅原则 + 四类通道表 + 同步三层含义分清）。同日增补（⑨）：新增 §5.8「L0 传输模型：就绪与完成两族」——reactor/proactor 接口分野表（缓冲所有权/背压/取消三差异）、三框架先例实读（BW event_poller.hpp:95-137 + event_dispatcher.cpp:370 select-only、KBE poller_epoll.cpp/poller_select.cpp、skynet socket_epoll.h:20）、apollo 三处 L0 现状（B 栈 include/apollo/network/transport/reactor.hpp + reactor.cpp:57-59/:92 poll 轮询、栈 A native_adapter.cpp:778/:786/:1027-1075 IOCP+epoll 建而事件循环未消费、ipc async_io.h:12-25/:201+ IoMultiplexer 完成模型三后端 + vector 拷贝）、发送环提交语义定形（两族通吃 + 水位判据不变 + 取消 ABORTED）、运行模式（epoll M1 首刀/io_uring M2+ 基准准入不开 SQPOLL/IOCP 不进路线图）；摘要 9、§6 决策表 L0 传输模型行联动。同日增补（⑩，design-gap-inventory B5 批）：§4.3 上行限流（#8——四通道参数表/三级处置/violation_score 单桶）；§5.9 客户端通道安全与加密库选型（#4——威胁模型裁域/X25519+AEAD+HKDF 握手序列/算法分级禁用表/OpenSSL 单一源定案）；§5.10 出站 HTTP 三裁决（#11——curl 进 vcpkg/Drogon 删/异步交接/SSRF 白名单）；§1 现状表 HTTP 行、§6 决策表两行、§7 P3 观测行接出引用、摘要 10 联动；观测接出细则落 logging.md §5.1（#10）。2026-09-30 勘误回填（architecture-review §19.1 P-1/§19.2 P-2，设计批粘贴）：§5.10 裁决 3「既有 event_loop.cpp …curl_multi」→「按新建计」（C-57：event_loop.cpp 为纯 poll reactor、与 curl 零接线——工作量口径修正，裁决方向不变）；§5.10 现存段行数 2541→5048（websocket.cpp 1193 + 三公共头 1314 登记时漏计）。*