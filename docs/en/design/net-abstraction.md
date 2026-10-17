
# Apollo Network Layer Abstraction Design (net-abstraction)

> Status: design draft (under review). Goal: give game logic a "thin" network API, sinking connection management, codec, reconnection, and backpressure entirely below it; and answer the boundary question of "thin self-developed wrapper vs adopting Aeron". Mutually referenced with `docs/design/attribute-sync.md` (QoS channels/budget), `docs/design/scripting-lua.md` (scripts see only the three-piece set), `docs/analysis/ssengine-reference.md` (the sdnet old design).

---

## Executive Summary

1. **Game logic sees only four actions**: `send / subscribe / state / close` — handshaking, frame delimiting, heartbeats, reconnect-and-resume, watermarks, thread migration all sink below; callbacks always land on the owning scene thread (the same line as the single-writer discipline in attribute-sync §10 and scripting-lua §2). The existing `include/apollo/net/session.h` is in "system-library form" (`onRecv` raw bytes, returns the consumed byte count, session.h:59) — a whole session layer short of "game-usable".
2. **Delete sdnet_adapter.h**: it **hand-writes a fake SSCP namespace and ISSBase/ISSConnection interfaces** inside the apollo repo (`include/apollo/net/adapters/sdnet_adapter.h:12-80`; `#define APOLLO_USE_SDNET` with no build configuration linking the real SSEngine) — an adapter in name, an empty shell in fact, the same disease as the fake-fruit findings in architecture-review. sdnet's real gems (DelaySend cross-thread delivery, GetSendBufFree watermark visibility, the GATE gateway mode) are folded into this design as design points.
3. **Aeron conclusion (closed, architecture-review §15.2): not for the client; the inter-process bus is self-developed.** Aeron's positioning is machine-to-machine messaging over UDP unicast/multicast and IPC (term buffers, offer/poll + BACK_PRESSURED, flow control + NAK, Archive replay, Raft clustering); it **does not do massive TCP/WS player long connections** — the client path gets a self-developed thin wrapper; the inter-process bus is also settled as **self-developed** (§15.2: nng retired, a second coexisting inter-process communication stack forbidden), with Aeron demoted to a semantic blueprint (the §5.2 absorption list stands).
4. **Backpressure is a first-class citizen**: the four-level watermark (ok/soft/hard/cut) + `trySend` explicit return codes (ACCEPTED/BACK_PRESSURED/TRIMMED_LOW) — "never queue, never block, hand the decision back to the caller" is Aeron's `offer()` philosophy; the drop order is decided by the §5 priority system of attribute-sync (low importance / far distance trimmed first).
5. **Four QoS channels** (movement unreliable / attributes reliable / events / control) run over the same session, forming two levels of independent control with attribute sync's token-bucket budget: the "budget" decides how much should be sent, the "watermark" decides whether anything more may be sent — both decided on the single-writer thread, lock-free.
6. **The NNG wrapper is settled for retirement** (architecture-review §15.2 retirement list): `modules/protocol/nng_wrapper` is deleted along with the five retirement items, avoiding two coexisting inter-process communication stacks (the same duplication disease as "four configuration systems" — no third instance gets created).
7. **The internal network layer is delivered on an evolution ladder (§5.6, added 2026-09-29)**: the meaning of self-development = owning **a kernel that can be optimized sustainably** (Mercury same-form — BigWorld's internal network layer evolved over twenty years rather than being delivered in one shot) — M0 semantics set in stone (P1, with this design) / M1 minimal correct kernel (P3) / M2+ continuous optimization (no deadline, small batches on demand); optimization happens only inside the layer, with zero perception by consumers.
8. **Inter-process connection facilities opened to upper layers (§5.7, added 2026-09-29)**: `InterServerLink` = a first-class public facility for stable inter-process connections — a connector (blueprinted on Mercury TCPConnectionOpener: non-blocking connect + timeout + errno failure classification; mechanism without retry), unified reconnection management (exponential backoff + G-1 fleet-event coupling — converging the anti-pattern of BW LoggerEndpoint's consumers each writing their own reconnection), disconnection in-flight semantics split by invoke_mode (OneWay dropped / ReliableEvent bounded queueing and resume / RequestReply timed out and not revived), peer restart distinguished by authority_epoch (≠ disconnect-reconnect), per-Link backpressure never trims; cross-server business and framework internals (RemoteEntityCall / G-2 mirror streams / collector push) consume **the same API family** — no raw sockets opened, no out-of-contract messages opened (Mercury's InterfaceMinder single-table flaw is not copied).
9. **The L0 transport two-family analysis landed (§5.8, added 2026-09-29)**: the interface/runtime-mode differences of the readiness (reactor: epoll baseline) and completion (proactor: IOCP/io_uring) families are set in form — the L0 interface settles on "**send-ring submission semantics**" (post_send/on_send_complete/post_recv/on_recv) rather than "event source + writev": reactor adapters translate internally into "readiness → fetch from ring and writev → synchronous return counts as completion", completion adapters submit straight to SQE/OVERLAPPED — both families are covered, the watermark machine's criterion (ring occupancy) changes zero, and cancellation semantics unify on ABORTED. All three framework precedents are reactors (firsthand reads this round: BW select-only / KBE epoll+select / skynet epoll+kqueue); the completion model is listed as M2+ with benchmark admission; **IOCP is off the roadmap** (servers are Linux-only; the `_WIN32` branch survives only as a compilability shim). The repo's three L0 attempts are inventoried in the same pass: the B stack poll (the base), stack A's IOCP+epoll skeleton (epoll built but the event loop never consumes it — the same family as C-18's dead inotify), and the ipc tree's IoMultiplexer completion-model interface (vector copy semantics, not copied).
10. **Inbound / security / outbound three-face closure (added 2026-09-30)**: inbound per-session rate limiting + violation_score as one risk-control bucket (§4.3 — the mirror of the outbound budget; over-limit frames never enter the scene thread); client channel security = CryptoFilter (positioned after compression) + X25519/AEAD/HKDF handshake + **OpenSSL as the single crypto source** (§5.9 — defends against network third parties, not against players holding the client); outbound HTTP = curl closure + Drogon branch deletion + synchronous APIs barred from scene threads + destination whitelist (§5.10); observability egress "three prohibitions" and the single exporter/tail/trace subset settled in logging.md §5.1 (#10).

---

## 1. Current-State Inventory (code-reading findings)

| Component | Location | Current state | Verdict |
|---|---|---|---|
| Connection/Listener/EventLoop | `include/apollo/net/connection.h` `listener.h` `event_loop.h` | IO abstraction skeleton, right direction | **Keep as the L0 prototype** |
| Session | `include/apollo/net/session.h:37-128` | Callback-style: `onRecv(data,len)` raw bytes + returns consumed byte count; send passes straight through to Connection; heartbeat is only get/set timestamps, no implementation | Wrong interface form (raw TCP semantics leak upward); **redesign as L2** |
| native_adapter | `include/apollo/net/adapters/native_adapter.h` | Self-developed epoll skeleton | Keep as the only real L0 implementation |
| sdnet_adapter | `include/apollo/net/adapters/sdnet_adapter.h:8,12-80` | **Fake SSCP**: after `#define APOLLO_USE_SDNET` it hand-writes `namespace SSCP`, ISSBase/ISSConnection/version structs/error codes — no configuration in the repo links the real SSEngine | **Delete** (see §6) |
| HTTP/WebSocket/RPC | `include/apollo/net/http/`, `websocket.h`, `rpc.h` | Peripheral capability prototypes (outbound-HTTP current-state evidence in §5.10) | Outbound HTTP ruling in §5.10 (added 2026-09-30); the websocket tree is a candidate for the next audit round |
| protocol module | `modules/protocol` (codec/messages/nng_wrapper/socket) | NNG wrapper + self-developed message encoding; **no .proto files anywhere in the repo** | codec merges into L1; nng_wrapper per the §6 decision |
| SSEngine sdnet | `/home/cui/workspaces/SSEngine` (for comparison) | `Send/DelaySend` (cross-thread delivery), `GetSendBufFree` (watermark visibility), `SetBufferSize`, the GATE variant | Ideas absorbed (§5.4) |

Core judgment: **apollo does not lack IO-layer parts; it lacks L1/L2** — frame format, session semantics (seq/ack/heartbeat/reconnect), channels and backpressure. These three are the dividing line between a game network layer and a "network library".

## 2. Target Form: the API Game Logic Sees

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

Hidden list (invisible to game logic): connection establishment and handshaking, encryption, frame delimiting and CRC checking, heartbeat keepalive, seq/ack and reconnect-resume, watermark and trim, cross-thread migration (send may be called from any thread, internally enqueued into an MPSC ring); the Lua side sees even less — only the three-piece set of `apollo.net` (scripting-lua.md §8).

## 3. Layered Architecture

```
L3  Logic Facade      GameConnection（§2）——游戏逻辑/Lua 唯一入口
L2  Session           seq/ack、心跳、重连续传(resume token)、四通道队列、水位机
L1  Framing/Codec     帧头(16B: magic+len+seq+crc32+ch/flags) + protobuf + zstd(>256B)
L0  Transport         adapters: epoll/io_uring(native) | IOCP | Aeron | IPC | (测试: loopback)
```

### L1 Frame Format

Aligned with sdpkg's "fixed header + checksum" direction and correcting its strength: `magic(2B) | header_len/ver(1B) | channel+flags(1B) | seq(4B) | len(4B) | reserved(2B) | crc32c(2B header 头校验)`, with payload verification crc32c riding the frame tail (or the len field + a trailing 4B CRC). Changes vs sdpkg: add **seq** (the basis for reconnect and reorder detection), and CRC32C replacing `(len^0xBBCC)&0x88AA` (ssengine-reference.md §4.5 verdict: predictable, insufficient strength).

### L2 Session Semantics

- **seq/ack**: an independent seq per channel; acks compress into cumulative confirmation + an optional bitmap (SACK thinking), piggybacked on upstream traffic.
- **Heartbeat**: L1 ping/pong frames, with the Session layer maintaining the RTO; timeout → Closed(Timeout), connection cleanup.
- **Reconnect-and-resume**: on disconnect the Session keeps the un-acked reliable queue + a resume token (TTL); the reconnect handshake carries the token and the server compares `acked_seq` — **the same model as attribute sync's ViewerState.acked_seq, two instances of it** (attribute-sync §3); if expired, a brand-new session with attributes going through enter-view snapshots — a natural backstop.
- **Four-channel queues**: 4 queues per session (§4), enqueued by the single writer (the scene thread), flushed by the IO thread, MPSC ring (reusing `include/apollo/utils/loop_buffer.h` and the `data_queue.h` port).

### Threading Model

```
IO 线程（1-2 个）：epoll/io_uring 事件 → 读入 per-conn 缓冲 → L1 解帧/校验/解压
                   → payload 按 (conn → entity) 投递到 owning 场景线程任务队列
场景线程（单写者）：处理上行 → 改属性/状态（attribute-sync §10 六阶段）→ 产出下行
                   → send() 入 MPSC 环（含水位判定，零锁）
IO 线程 flush：按水位预算从环上取帧 writev
```

Key disciplines: **callbacks happen only on the scene thread** (the precondition for game logic being lock-free); **the IO thread never holds game-data locks** (it only moves bytes); for the send direction the game thread is the producer and the IO thread the sole consumer.

## 4. QoS Channels and Backpressure

### 4.1 Four Channels (aligned with attribute-sync §5.3)

| Channel | Reliability | Order | Typical load | Congestion behavior |
|---|---|---|---|---|
| `movement` | unreliable | none (timestamps, take newest) | position/orientation/velocity | **send newest only**: directly overwrite unsent older frames of the same entity |
| `attributes` | reliable | ordered | attribute deltas/snapshots | only batches of low-priority entities may be trimmed (§5 priority); no sequence skipping |
| `events` | reliable | ordered | AOI enter/leave, battle reports, floating text | never dropped; when full, push the watermark up |
| `control` | reliable | ordered | handshake/heartbeat/ack/resume | never dropped; a separate small queue guarantees it never starves |

### 4.2 Four-Level Watermark and Behavior Table

| Watermark | Criterion (per-channel send-ring occupancy) | Behavior |
|---|---|---|
| ok | less than 50% | full send |
| soft | 50–75% | movement switches to "send newest only"; attributes start trimming from Priority::LOW |
| hard | 75–95% | trySend returns TRIMMED_LOW/BACK_PRESSURED for LOW/NORM; triggers the slow path: report to monitoring + automatic attribute budget downshift (token bucket refill rate lowered, attribute-sync §5) |
| cut | at or above 95% | the session's reliable queues freeze new enqueues (control frames excepted); lasting N seconds → disconnect (the client will reconnect + resume on its own) |

- **Standardized watermark query**: `Watermark::query(conn, ch) -> level` replaces the raw byte count of sdnet's `GetSendBufFree()` — folding the old engine's "watermark visibility" idea into a typed API (ssengine-reference.md §3 verdict).
- **Two-level control, orthogonal**: attribute-sync's token bucket decides "how many bytes should go out this tick" (shaping); this layer's watermark decides "may anything still be enqueued" (backpressure); the former is active shaping, the latter passive circuit-breaking, and they meet at the hard watermark (budget downshift).

### 4.3 Inbound Side: per-session Rate Limiting and Risk-control Counting

All of §4.1/§4.2 is the **downstream** (server→client) channels and watermarks; the inbound side previously had only one line, "intent only, the server decides" (sdk-contract §2.3 msg comment) — zero defenses against a malicious or buggy client flooding upstream packets (BW's LoginConditions-style login-time admission does not cover the sustained period, §7); this section fills the gap. The two directions mirror each other: downstream defends "getting it out" (budget + watermark), inbound defends "getting it in" (rate limiting + risk control).

- **Enforcement point**: in the IO thread after L1 frame decode, **before** delivery to the owning scene thread — over-limit frames never enter the scene thread at all (the single writer's tick budget must not be eaten by an upstream flood); the counters are per-session with the IO thread the single reader/writer, zero locks.
- **Parameter table** (defaults, overridable by login configuration; the caps = expected steady state ×3 magnitude — a normal client never touches them):

| Channel | Expected steady state | msg/s cap | byte cap | Over-limit handling |
|---|---|---|---|---|
| movement | client reports at 20Hz | 60 | 2KB/s | drop old frames (same semantics as the downstream "send newest only"; position takes the newest, no notification needed) |
| attributes | ACK piggyback ~10/s (the client never sends SetAttribute) | 120 | 4KB/s | drop + violation_score |
| events | skill/interaction intents ~5/s | 30 | 4KB/s | drop + throttle_notice back on the control channel (the normal client's backoff signal) |
| control | heartbeat 1-2/s + ack piggyback | 20 | 1KB/s | **hard over-limit disconnects immediately** (the control channel tolerates no flooding — flooding control is an attack, not noise) |

- **Total session byte rate**: an 8KB/s steady-state cap + a 64KB burst bucket (token bucket — same form as the downstream attribute-sync §5.1, opposite direction).
- **Three handling tiers**: soft (single-channel over-limit → drop + count) / hard (total-bytes over-limit → drop + throttle_notice) / abuse (**violation_score** over threshold for N consecutive ticks → disconnect + a risk-control record, account-level, consumed by the login-app admission gate).
- **violation_score, one bucket**: rate-limit counters + L1 schema validation failures + attribute-sync §9 authority-correction counts go into **the same risk-control bucket** — "packet flooding" and "semantic illegality" weigh the same on the risk-control face (the entrance gate and the semantics gate share one exit).

## 5. Aeron Research and Trade-offs

### 5.1 Mechanism Overview (source/doc highlights)

- **Model**: Publication (sender, may be multi-active) ↔ Subscription (receiver); each sender connection a Subscription receives is called an Image, each Image with a sessionId — the primitive for multiplexing and stream isolation.
- **Transport**: the term buffer = a per-stream three-segment ring log buffer (sequential writes, indexed pages), written by a single writer and scanned by the reader — **the lock-free precondition is "one writer per buffer"**, isomorphic to apollo's scene-thread discipline.
- **Backpressure**: `offer()` is non-blocking, returning BACK_PRESSURED/ADMIN_ACTION/closed errors — no queueing, no blocking, the decision handed back to the caller; receiver-side flow control (min/median strategies aggregating multiple subscriber windows) + NAK-requested retransmission, with reliable/unreliable stream modes.
- **Periphery**: the Media Driver runs as a separate process or embedded (embeddedMediaDriver); IPC transport goes over shared memory (zero-copy between same-machine processes); the Archive module records streams for replay; Cluster mode replicates a state machine over Raft. Apache 2.0, mature C/C++ clients.

### 5.2 What to Absorb (whether or not the library is adopted)

| Aeron concept | Landing point in apollo |
|---|---|
| the offer() return-code philosophy | §2 `trySend` three return codes — upper layers handle congestion explicitly; "silently queue and die" is forbidden |
| term buffer single-writer ring segments | the data-structure blueprint for send/receive rings (the self-developed MPSC ring is designed directly on this) |
| per-stream sessionId isolation | §4's four channels are apollo's stream-isolation granularity |
| flow-control aggregated window | multi-viewer downstream aggregation: slow viewers pull down the whole batch's send rate (the AOI broadcast's per-viewer budget, attribute-sync §5) |
| IPC shared-memory transport | a candidate for same-machine inter-process channels (framed together with sdshmem, ssengine-reference.md §4.4) |
| Archive replay | P3: cross-server message audit / fault replay (optional) |

### 5.3 Conclusion: Where to Use It, Where Not To

| Path | Decision | Rationale |
|---|---|---|
| client ↔ server (massive TCP/WS long connections, tens of thousands of conns) | **self-developed thin wrapper** (this design's L0–L3) | Aeron does not do TCP long-connection access; the media driver's per-connection overhead, UDP's unreliability on the player-side network, and operational complexity all mismatch. The player path's problem is "per-connection session semantics", which is exactly the layer Aeron deliberately does not do |
| inter-process bus (gateway↔game↔world↔db-proxy, cross-machine + same machine) | **self-developed (settled, architecture-review §15.2)**, semantics copied from §5.2 (term buffer single-writer segments / flow-control aggregation / NAK / offer return codes) | after nng's retirement a second inter-process communication stack is forbidden; the self-development blueprint is ready-made — the §5.2 absorption list + §5.5 Mercury filter two families + udp_channel window retransmission |
| same-machine bulk read-only sharing (spatial snapshots / metrics) | an sdshmem-style scheme, settled in the same batch as the inter-process bus in P3 (§6 decision row / §7 P3 wired) | see ssengine-reference.md §4.4 |

### 5.4 Compared with SSEngine sdnet

| sdnet | This design's counterpart | Relationship |
|---|---|---|
| `DelaySend` (cross-thread delivery) | scene thread → IO thread MPSC ring | same idea, the ring structure modernized on the term-buffer blueprint |
| `GetSendBufFree` | `Watermark::query` four-level watermark | raw byte count → typed decision API |
| `SetBufferSize` | session establishment parameter (internal) | keep, hidden from upper layers |
| GATE variant | gateway-app (planned as apps/gateway-app) | same direction; the gateway-to-this-layer interface is the §2 API |
| sdpkg frame header | the L1 frame format | add seq + CRC32C (§3) |

### 5.5 The Mercury Filter Two Families → the L1 Frame Pipeline Blueprint

BigWorld Mercury's filter is **a frame-pipeline plugin system that has run for twenty years**, absorbed on four points:

- **Two-family interface** (packet_filter.hpp:37 send/:48 recv/:56 maxSpareSize; stream_filter.hpp:46 writeFrom/:55 readInto) unified into apollo's `FrameFilter`: `Reason encode(Frame&) / decode(Frame&) / reserve()` — Reason expresses "pass / truncate / need more bytes", and `reserve()` lets compression/encryption filters announce their expansion, avoiding per-frame reallocation.
- **The WS precedent proves "forbid a fifth network stack"**: BigWorld's WebSocket is a stream filter, not a separate stack (the packet_filter/stream_filter two families + mutable_stack configuration) — apollo's WS takes the same plugin slot and does not start a new transport layer.
- **The filter stack is declared by the contract**: the frame header's `ver` field + the contract messages.xml channel declarations (sdk-contract §2.3) → the generator produces the L1 assembly code; filter order locked across ends by the contract, never runtime string configuration.
- **Boundary discipline**: filters do only byte↔byte transforms (compression/encryption/obfuscation); seq/ack/retransmission windows stay in the L2 channel kernel — BigWorld's window retransmission lives in udp_channel, not in filters; this is the line between "plugin slot" and "session core".
- **Ownership and preconditions** (architecture-review §16.8.3-①): the FrameFilter interface and built-in plugins land in **the converged modules/net** — precedent: BW's filters live in the same library as the channel, constructed by injection (udp_channel.hpp:81/:139-140); protocol (generated codec) is injected via the L1 codec slot, and apps/gateway-app only assembles, never implements. **Precondition = C-29's convergence of the four network trees** (architecture-review §12.2's four-tree inventory, §15.2's "forbid a fifth stack" discipline) — the filters must not land as-is inside any of the existing four trees, otherwise the filter chain becomes the fifth place with network code.

### 5.6 The Internal Network Layer's Evolution Strategy: Establish the Kernel First, Optimize Continuously

BigWorld's Mercury is **an internal network layer that evolved for twenty years**, not a library delivered in one shot — the filter two families (§5.5), udp_channel window retransmission and other capabilities were layered on over years (source evidence in architecture-review §16.7.1), while the game-layer API stayed stable throughout. What apollo's self-development decision (§15.2) must learn is exactly this form: **self-development = owning a kernel that can be optimized sustainably**, not "write it once to benchmark twenty years" — adopting an external library hands the optimization cadence to someone else. apollo's internal network layer = this design's L0-L3 (the client/single-process path) + the inter-process path, **one set of semantics, one layer** (§2's four actions, the SendCode return codes, the four channels, the watermark machine, and the FrameFilter slot all serve both paths); the difference is only in transport and topology. The evolution ladder:

| Stage | Deliverable | Performance semantics |
|---|---|---|
| **M0 semantics set in stone** (P1, lands with this design) | four-action API, SendCode three return codes, four channels, watermark machine, FrameFilter slot — fixed the moment they land on the client/single-process path; the inter-process path consumes **the same semantics**, no second API face | none (an interface layer; what is set is semantics, not implementation) |
| **M1 minimal correct kernel** (P3 first batch) | the inter-process segment: frame delimiting + contract_route dispatch by id (sdk-contract §10.3) + backpressure watermark + unicast topology + **the InterServerLink connection facility (§5.7 — connection lifecycle/reconnection/opened to upper layers)**; the same-machine segment's shm form is settled in the same batch (§6 decision row — whether it enters the first version depends on same-machine deployment) | **correctness first, no performance targets** — early internal traffic is small; premature optimization has no target |
| **M2+ continuous optimization** (no deadline, small batches on demand) | window retransmission (udp_channel blueprint), flow-control aggregation (Aeron blueprint §5.2), filter plugin family expansion (encryption/audit), bundle deferred aggregation (one batch-and-flush per tick — attribute-sync §7.1's "at most one frame per client per tick" is its same form, generalized to inter-process), same-machine shm channel implementation (sdshmem candidate, G-2 mirror streams — form fixed at M1, implementation may follow later) | **every item gets a benchmark before work starts**; independent small batches, revertible |

Two disciplines guarantee the "sustainable":

- **Optimization happens only inside the layer**: consumers (attribute sync / scripts / business) depend only on §2's four actions and the channel semantics — kernel rewrites, transport swaps, and filter additions/deletions are invisible to them. Mercury's twenty-year evolution without a game-layer API change is the same fact; this is the long-term dividend of "game logic cannot see the network" (§2 hidden list), and the reason it exists.
- **Each level has its own acceptance object**: M0 is accepted on frozen semantics (interface review + the contract entering sdk-contract); M1 is accepted only on correctness (dispatch loses nothing and duplicates nothing, backpressure visible, disconnection detectable); M2+ admits every optimization on benchmark data — guarding at once against "optimization for optimization's sake" and "pretending to evolve".

Mapping onto §7's phasing: the original "P3 bus lands" wording becomes "**M1 goes live**"; M2+ occupies no phased milestone — they are the internal network layer's **steady-state work**, not roadmap nodes.

### 5.7 The Inter-Process Connection Facility (InterServerLink): Stable Connections Opened to Upper Layers

§5.6's M1 fixes the inter-process segment as the "framework-internal bus"; but **upper-layer business needs this layer too** — cross-server gameplay, matchmaking, chat, GM/ops tools, collector push all want the same few things: stable connections, disconnection awareness, automatic reconnection, backpressure. This section fixes it as a first-class public facility. BigWorld's form is exactly this: all server code uses Mercury directly, with entity mailboxes, backup streams, and tool processes sharing one network library (cellappmgr.cpp hits Bundle/Channel 25 times in a single file) — "opened to upper layers" is not a convenience added for apollo; it is this layer's original form upstream. Two new Mercury pieces of evidence serve as direct references: TCPConnectionOpener (the outbound connection state machine) and LoggerEndpoint (the exemplar and anti-example of an upper-layer consumer building its own stable connection).

**Vocabulary: the Link is level one; no session layer stacked on top**

- `InterServerLink` = connection lifecycle management over one physical bearer (TCP first; the same machine may swap to shm in P3 — §6's sdshmem row stands): dial/accept, handshake authentication, heartbeat and disconnection detection, reconnection, watermarks. QoS is not layered on top of the connection — the four-channel semantics (§4.1) are reused as **message-level flags** (reliable/unreliable/control belong to the message classification declaration); one Link carries one watermark set.
- The dial/accept roles are decided by the G-1 fleet topology (fixed in configuration state, no runtime negotiation); the handshake triple = **process identity (G-1 component id) + route_version + authority_epoch** — on reconnect, an epoch mismatch judges "the peer has restarted", which is **a different event** from disconnect-reconnect (the former requires clearing state and renegotiating, the latter only repairing the transport).

**The connector (mechanism layer): blueprinted on Mercury TCPConnectionOpener**

- Form: non-blocking connect + POLLOUT watch + a one-shot timeout timer (tcp_connection_opener.cpp:53/:99-101); on the success path it checks SO_ERROR, hands the endpoint to the channel factory, and calls back `onTCPConnect(channel)` (:113-146); failures are classified by errno into typed Reasons — ECONNREFUSED/ENETUNREACH → NO_SUCH_PORT, ETIMEDOUT → TIMER_EXPIRED (:186-231). apollo copies this form (failure classification shares one philosophy with the SendCode return codes: typed reasons, no guessing on the caller's behalf).
- **The connector carries no retry of its own** (Mercury's same choice — on failure it just calls back; retry is policy, not mechanism): retries belong to the Link manager, exponential backoff + **fleet-event coupling** — a G-1 death notification stops dialing that process immediately (never dial a dead process), a revival notification restarts the backoff.
- The anti-example is BW itself: LoggerEndpoint's reconnection policy is hardcoded per consumer (a cap of 3 consecutive failures, logger_endpoint.cpp:672-703) — apollo converges this into one place, the Link manager; consumers receive events only and write no policy.

**In-flight semantics during disconnection (split by invoke_mode)**

| Message mode | While disconnected | After reconnect |
|---|---|---|
| OneWay | drop or queue, per the message flags declaration (default drop — cross-server "best-effort" semantics) | — |
| ReliableEvent | queue (**bounded**: cap + watermark) | seq resume (the inter-process version of §3's L2 resume); peer restart (epoch mismatch) → clear the queue + upper-layer adjudication |
| RequestReply | the suspended future times out as usual | reconnection does not revive timed-out requests (idempotency belongs to the idempotent field — declared by the caller) |

- The bounded-queue precedent and lesson share a source: BW LoggerEndpoint's send enqueues into a bounded buffer while disconnected/congested, dropping and erroring past maxBufferedSize — **"queueing while disconnected" and "backpressure" are two names for one mechanism, and both must be bounded** (unbounded queueing = deferring backpressure into a memory bomb).
- Three event callbacks are opened to upper layers: `onLinkUp / onLinkDown(reason) / onPeerRestart(epoch_old→epoch_new)` — cross-server business implements its own session-recovery policy inside onPeerRestart (resend queries / renegotiate); the framework does not do it for them.

**Backpressure semantics (the dividing line between server-to-server and client)**

- The four-level watermark (§4.2) carries over, but **server-to-server never trims**: the client side's "low priority may be dropped" is a game-experience tradeoff; each internal message's drop eligibility belongs to its **message-class declaration** — G-2 mirror streams may declare old frames droppable (journal offsets backstop), RemoteEntityCall is non-droppable by default.
- The offer return-code philosophy is unchanged: on BACK_PRESSURED the caller decides (queue cap / downshift / assert); the framework never decides for it.
- Per-Link watermarks are independent: a slow process drags only its own Link, not other process pairs (the inter-process version of Aeron's per-image isolation, §5.2).

**The form and boundary of what is opened to upper layers**

- API family = the inter-process instance of §2's four actions (send/subscribe/state/close) + connection-lifecycle callbacks; **upper layers and framework internals consume the same facility**: cross-server gameplay / GM tools / collector push (logging.md §5) and RemoteEntityCall / route control / G-2 mirror streams are same-source, with no second API face — M0's "one set of semantics, one layer" honored in the inter-process domain.
- Obtaining connections = **named endpoints within the fleet topology** (G-1 service discovery); dialing arbitrary addresses is not opened — cross-process targets must be enumerable by the fleet (auditable by ops, preventing accidental connections to targets outside production).
- **Not opened**: raw sockets (users want stable-connection semantics, not sockets); runtime string protocol registration — upper-layer custom messages enter the contract's internal domain (sdk-contract §11, the 900+ segmentation's jurisdiction); Mercury's InterfaceMinder single-table flaw (recorded in the RemoteEntityCall block) is not copied.

**Mercury comparison and trade-offs**

| Mercury | apollo counterpart | Trade-off |
|---|---|---|
| TCPConnectionOpener (connection state machine + errno failure classification) | the Link connector | **copy the mechanism**; retries stay outside (same as Mercury — mechanism and policy separated) |
| LoggerEndpoint reconnection (per-consumer, hardcoded cap :672-703) | Link manager unified backoff + fleet-event coupling | **do not copy the scattered form** — policy converges in one place, consumers only receive events |
| Channel TCP/UDP dual mode + window retransmission | Link + M2+ window retransmission | windows belong to M2+ (§5.6 evolution ladder) |
| Bundle batched sending | M2+ bundle deferred aggregation | same form (listed in §5.6) |
| Interface/InterfaceMinder named method table | contract internal-domain method ids | **do not copy** (the single-table flaw, §7 RemoteEntityCall block) |
| the filter two families | §5.5 FrameFilter | as before |
| all server code using Mercury directly | upper layers and the framework sharing InterServerLink | this section's fixed point |

> 2026-10-12 (P4-4 batch ⓪): **the linkrpc correlation primitives are delivered** (modules/net/linkrpc, contract 940/941) — request/response correlation + a synchronous-wait bridge, payload opaque (body = a full MessageCodec frame; business semantics belong to each process's handler). RPC ownership closes per this section's "not opened" row: upper-layer custom messages do not go through string protocols; the correlation envelope (corr_id + bytes) is the only M1 RPC face; the five nng consumer surfaces (scoped in P4-4) migrate on this, with the first case — gateway admission — already swapped (login `--m1-port` / gateway `--login-m1-port`). Thread discipline: dispatch callbacks run on the host pump thread, and synchronous waits must never pump-and-wait on their own thread — the seams with their own pump threads (the admission service / the later login↔base and manager control planes, same form) follow this.

### 5.8 The L0 Transport Model: the Readiness (reactor) and Completion (proactor) Families — Interfaces and Runtime Modes

§3's layering diagram lists epoll/io_uring/IOCP slots at L0, and §15.2 fixes the B stack as the sole base with the evolution path spelled out — but "where the two families differ in **interface shape and runtime mode**, and how the L0 interface carries both" previously had zero analysis (this section supplies it). Without fixing this layer, swapping backends later means a rewrite: IOCP is mature and io_uring is Linux's new direction; the interface seam must be cut right at design time.

**The two-family divide (interface layer)**

|  | Reactor (readiness notification) | Proactor (completion notification) |
|---|---|---|
| syscall family | select/poll/epoll/kqueue | Windows IOCP, Linux io_uring (SQ/CQ) |
| semantics | "the fd is readable/writable — go do it yourself" | "hand me the buffer; I will notify you on completion" |
| callback signature | `(fd, readable\|writable)` | `(bytes_transferred, error)` |
| buffer ownership | application-held; non-blocking short reads/writes looped by hand | **kernel/queue-held until completion** — must not be reused before completion |
| per-IO cost | wait amortized in batch + one syscall per read/write | one enqueue and one dequeue each for submit/completion (io_uring with registered buffers can go zero-copy) |
| backpressure | not built in (stack the watermark yourself) | **submission depth is naturally backpressure** (SQE depth / outstanding IO count) |
| cancellation | not a problem (synchronous read/write) | must be defined (in-flight ops at close: io_uring ASYNC_CANCEL / IOCP CancelIoEx) |

io_uring is a superset of both families (`IORING_OP_POLL_ADD` can serve as pure readiness, buffered reads/writes go the completion path, the provided buffer ring gives the receive direction zero-copy) — but **the payoff exists only on the completion path**: using it merely to emulate a reactor is adopting it for nothing.

**Three framework precedents: all reactors (firsthand reads this round)**

- BW Mercury: the `EventPoller` abstract base class (event_poller.hpp:95-137, doRegisterForRead/Write + processPendingEvents pure virtual) + **a select-only implementation** (event_dispatcher.cpp:370 self-describes select in a comment; lib/network has no second poller file).
- KBEngine: the poller abstraction + epoll/select dual backends (poller_epoll.cpp 140 lines / poller_select.cpp 256 lines).
- skynet: the socket thread's SP_* macro abstraction (socket_epoll.h / socket_kqueue.h, :20 `epoll_create`).
- Conclusion: **MMO mainstream has sat in the readiness family for twenty years** — the bottleneck was never syscall density but per-connection session semantics (seq/ack/watermark/reconnect, i.e. L1/L2, §1's core judgment); the completion model is the battleground of Aeron/Seastar-class high-throughput messaging layers. apollo taking reactor as the baseline is not backwardness — it is the same form as every precedent; the completion model is an M2+ evolution item (below).

**The apollo repo's current state: three L0 attempts (firsthand reads this round; including one outside the C-29 inventory boundary)**

| Attempt | Location | Model | Form and problems |
|---|---|---|---|
| B stack (the only one in the default build, §15.2 base) | include/apollo/network/transport/reactor.hpp + modules/net/tcp/src/reactor.cpp | poll(2) reactor | AddSocket/ModifySocket/RemoveSocket + EventCallback(fd, events) + EventLoop on a dedicated thread (the reactor interface shape is right); reactor.cpp:57-59 POLLIN/POLLOUT/pollfd typedef seam, :92 `poll(fds, n, 1000)` polling wait; socketMutex_ full-table lock; Windows branch missing (`#ifndef _WIN32` includes poll.h) |
| stack A | modules/net/rpc/src/adapters/native_adapter.cpp (1109 lines) | IOCP + epoll dual-backend skeleton | initialize's dual macro branch (:778 CreateIoCompletionPort / :786 epoll_create1) + PostQueuedCompletionStatus exit notification; **but ioThreadProc is actually full-connection polling** (:1027-1075, every 100ms it traverses the listeners/connectors/connections tables calling handleRecv/handleSend — the epoll fd is built but the event loop never consumes it, the same family as C-18's FileWatcher "dead inotify"); the IO thread pool defaults to hardware_concurrency() = 4 |
| ipc tree (default OFF, §15.2 already flags next audit round) | include/apollo/ipc/async_io.h (572 lines) | **completion-model interface** (the only one in the repo) | `IoMultiplexer`: postRead/postWrite/postAccept/postConnect + IoEvent{result, error, op} completion callbacks + runOnce; three-backend detection macros (:12 IOCP / :16-18 `__has_include(<linux/io_uring.h>)`→URING / :21-25 EPOLL fallback / KQUEUE); **AsyncOp carries `std::vector<uint8_t> buffer` by value copy** — a completion-model interface using copy semantics; neither zero-copy nor the physical basis for backpressure was built |

(Stack A's "epoll built but unused" and the ipc tree's interface are incremental facts outside the C-29 four-stack inventory boundary — the ipc tree was already the registered audit object for the next round; both are recorded in the architecture-review registry.)

**L0 interface fixed: send-ring submission semantics (the key to covering both families)**

- §6's current convergence wording "event source + writev + watermark" is a **reactor shape**. Fixing the interface that way forces IOCP/io_uring into degenerate use (re-wait for writability after every completion then write — the classic anti-pattern on IOCP).
- The correct form: L0 exposes **ring submission**, not sockets: `post_send(ring slice) / on_send_complete(n) / post_recv(buffer) / on_recv(buffer, n)`. A reactor adapter translates internally into "epoll readiness → fetch from the ring and writev → synchronous return counts as the completion callback"; a completion adapter submits the slice straight to SQE/OVERLAPPED. The L2/threading model (§3: scene thread single-writer enqueues into the ring, IO thread the sole consumer flushing) has the same shape under both families — **the ring is the submission unit**, a deepening of §5.2's term-buffer blueprint.
- **Buffer ownership rule**: under the completion model a send slice must not be reused before completion → ring capacity = the cap on outstanding bytes → §4.2's watermark machine criterion is "ring occupancy" under both families, zero change — the direct dividend of building backpressure on the ring rather than on the socket. The ipc tree's AsyncOp vector copy is precisely the interface form that never thought this step (copying buys safety while discarding zero-copy, the physical basis of backpressure, and the watermark semantics) — **do not copy**.
- **Cancellation semantics, one unified wording**: at close() in-flight operations complete as usual but their callbacks carry ABORTED (io_uring IORING_OP_ASYNC_CANCEL / IOCP CancelIoEx + ERROR_OPERATION_ABORTED mapped to one Reason), resources return to the ring, and the connection state machine does not wait on in-flight — joining the §5.7 disconnected in-flight semantics table.

**Runtime modes (per backend)**

- **epoll (the M1 baseline)**: poll→epoll is M1's first cut (the B stack's reactor.cpp:92 1000ms poll wait is the replacement target); one epoll instance per IO thread (multi-thread sharing of a single fd's event dispatch needs normalization — stack A's current state must change); level-trigger + batched writev aggregation; flush at tick boundaries (§3's threading model unchanged).
- **io_uring (M2+, on demand)**: **one ring per thread** (the SQ/CQ lock-free precondition is ring privacy); **no SQPOLL** (the kernel polling thread steals cores from the game server — the harm outweighs the benefit); batch submit+reap each round; provided buffer ring optional (zero-copy receive). **Benchmark admission** (§5.6 discipline): first measure syscall counts and tail latency against epoll with real connection counts/packet shapes; switch only if it wins — and the switch itself is an in-layer replacement (guaranteed by this section's interface form).
- **IOCP (off the roadmap)**: servers = Linux as the only deployment target; Windows exists only as dev machines — stack A/native_adapter's `_WIN32` branch is kept as a **compilability shim** (preventing dev-machine build rot), with no performance promises and no optimization investment. Likewise kqueue gets no investment (macOS dev machines).

**Joining §5.6's evolution ladder**: M1 = epoll (including the poll→epoll first cut); M2+ = io_uring on demand (benchmark admission); IOCP/kqueue = non-goals (shim level). Once the interface is fixed on this section's "ring submission semantics", every replacement is an in-layer replacement with zero perception by consumers — §5.6's "optimization happens only inside the layer" discipline honored at L0.

### 5.9 Client Channel Security and Crypto Library Selection

sdk-contract §10.5's frame-layer diagram `[Encryption P3]` placeholder is fixed here: encryption = **a CryptoFilter of the FrameFilter family** (the §5.5 slot; the filter order is already fixed in sdk-contract §10.5 — **after compression**: ciphertext is incompressible, compress first then encrypt); the handshake rides the control channel, and keys never leave the filter layer.

- **Threat model (pick the enemy before the weapon)** — defends against: eavesdropping (plaintext sniffing on public networks) / replay (captured frames re-sent) / reconnect hijacking (stealing the resume token to impersonate a continuing connection) / man-in-the-middle (handshake tampering); **non-goals: client reverse engineering and cheating** — the client binary is reversible and any client-side key is extractable; anti-cheat relies on server-authoritative adjudication (attribute-sync §9) + recomputed hash chains (battle-determinism §5), not channel encryption. Encryption defends against "a third party on the network", not "the player holding the client" — this statement heads off the common misdirected investment of "encryption = anti-cheat".

**Handshake sequence (the TCP main path; authentication and key negotiation done in one pass)**:

```
C → S  ClientHello { login_token, nonce_c, pub_c(X25519), aead[] }   // control 通道
S → C  ServerHello { nonce_s, pub_s(X25519), aead_selected }
双方    shared = X25519(own_priv, peer_pub)
        session_key = HKDF-SHA256(shared, salt = nonce_c‖nonce_s,
                                  info = "apollo-session-v1"‖session_id)
此后    全帧 CryptoFilter：AEAD(session_key, nonce96 = nonce_c_hi64 ‖ seq32)
```

- **login_token is issued by login-app** (account authentication and game-session duties separated: the token has a short TTL + single use; game processes verify the signature only and never touch the account DB; KBE loginapp splits duties the same way).
- **The per-frame nonce is derived from seq** (nonce96 = the handshake nonce's high 64 bits ‖ the frame seq's low 32 bits): seq is monotonic (§3 L1), guaranteeing the nonce is never reused under the same key — GCM nonce reuse is catastrophic failure; seq and encryption are symbiotic on this point.
- **Anti-replay**: before AEAD decryption, consult the receive sliding window (bitmap, the L2 SACK same idea) — out-of-window/duplicate seq dropped + violation_score (the §4.3 same bucket).
- **Forward secrecy**: every connection (including resume reconnects) redoes ECDH — resume restores the L2 seq/queue state, **never the old keys**; the token is single-use (void after use) + handshake nonce freshness blocks a hijacker's handshake race.
- **Three key tiers**: login_token (account domain, login-app) / session_key (connection domain, freshly negotiated per connection) / per-frame nonce (frame domain, seq-derived) — HKDF derives one way, never backwards.

**Algorithm tiering table** (goes into the contract doc and review red lines):

| Purpose | Selection | Notes |
|---|---|---|
| key agreement | X25519 | mature constant-time implementations |
| symmetric encryption | AES-128-GCM (first choice, AES-NI) / ChaCha20-Poly1305 (fallback, clients without AES-NI) | unified AEAD form |
| key derivation | HKDF-SHA256 | |
| digest | SHA-256 | SHA-1 only for the WS handshake's legacy protocol (RFC 6455's existing semantics) |
| **forbidden** | MD5, SHA-1 (new code), RC4, DES/3DES, bare ECB, CBC without MAC | the library still offering it ≠ usable |

**Crypto library: OpenSSL as the single crypto source (settled)** — vcpkg.json:6 already depends on it directly; websocket.cpp:38 SHA-1 (the WS handshake) and modules/net/CMakeLists.txt:135-160's built-in branch `find_package(OpenSSL)` are the existing consumption surface. **No second library** (libsodium/Botan: zero hits repo-wide, keep it that way) — the cryptography edition of the "forbid coexistence" discipline (§15.2): two crypto libraries = two CVE surfaces + split algorithm behavior. Usage discipline: only the EVP high-level interface (never the low-level primitives), tag/ciphertext comparison via `CRYPTO_memcmp` (constant-time), randomness only via `RAND_bytes` (home-grown entropy sources forbidden); version-locked to OpenSSL 3.x LTS.

- **The WSS fallback form**: WS browser clients get their encryption from wss (TLS), coexisting with the TCP+CryptoFilter dual form — settled with gateway-app in P3; the TCP custom handshake is the main path (controllable frame overhead + a unified filter stack), and wss is only the browser-side fallback.

### 5.10 Outbound HTTP and Third-Party Egress

Server-side **outbound** HTTP (third-party login verification / payment callbacks / push) is a real P3 need; the current state is "5048 lines written, all stubs in the default build" — modules/net/http (rest_client.h 343 + rest_client.cpp 734 + http.cpp 816 + event_loop.cpp 648 + websocket.cpp 1193 + three shared headers 1314; the last two were miscounted at registration time — architecture-review §18 errata), rest_client.cpp:13 `#ifdef APOLLO_HAS_CURL`, :16 `APOLLO_CURL_STUB 1` (no curl in vcpkg.json — the same family as C-45's macro-gated MySQL); zero production consumers (only examples/tests/docs). §1's current-state table said "handled separately" — this section is the ruling.

**Three rulings**:

1. **Dependency closure: curl into vcpkg, rest_client becomes a real implementation** — libcurl is the de facto standard (chunked/redirect/TLS/proxy/HTTP2 all self-contained); a self-written HTTP client is a negative asset; curl's TLS backend pairs with OpenSSL (§5.9's single crypto source unbroken). Deleting the `APOLLO_CURL_STUB` stub path happens with the code batch (frozen surface); the direction is fixed here.
2. **The Drogon alternative branch is deleted** (CMakeLists.txt:81-91 http, :124-133 websocket, both places) — the direct object of §15.2's "forbid coexistence": a web framework and an in-house client in dual form, both seeds of a "fifth network stack". The admin exporter does not need Drogon (logging.md §5.1: Prometheus text is a few dozen lines of thin layer).
3. **Threading model: synchronous APIs barred from scene threads** — RequestOptions' default timeoutMs=30000 synchronous semantics are the same family as SQL/Redis: always via scripting-lua §8's async handoff (the execution layer **counted as newly built** = curl_multi multiplexing on IO threads — the tenth-round audit (architecture-review §18 C-57) measured the existing event_loop.cpp as a poll(2) single-threaded reactor with zero relation to and zero wiring of curl, so it **does not constitute an "existing" execution layer**; the original wording here is corrected by errata; completion callbacks carry request_id back to the scene thread at tick boundaries); responses do not enter the current tick's rulings (battle-determinism §1 "async never enters rulings" — the landing point is a future tick's input).

**Boundary**: the outbound direction is opposite to the client's four channels — no shared channels, no shared session; a **destination whitelist** (domains/ranges into configuration, not addable at runtime) — the same discipline as §5.7's "no dialing arbitrary addresses" (the SSRF surface: outbound targets a script can trigger must be enumerable). The modules/net/{http,websocket} code audit (stub quality / test-asset disposition) belongs to the next audit round's candidates (registered); this section rules only the design surface.

## 6. Decision List (Keep / Delete / Introduce)

| Object | Decision | Basis |
|---|---|---|
| Connection/Listener/EventLoop | **Keep** as the L0 prototype, interface converged to "event source + writev + watermark" | right direction; what is missing is the two layers above |
| `sdnet_adapter.h` | **Delete** | fake SSCP (sdnet_adapter.h:12-80), never linked the real library, empty-shell code; the gems already distilled into design points (§5.4) |
| `session.h` current interface | **Redesign** (the callback form is not kept) | onRecv raw bytes leaks TCP semantics to game logic — the number-one leak this design eliminates |
| `modules/protocol` codec/messages | **Merge into L1**, codec moves to protobuf (the same contract source as attribute-sync §7 and sdk-contract) | no .proto anywhere in the repo; hand-written encoding cannot support multi-end SDKs |
| `nng_wrapper` | **Settled for deletion** (one of architecture-review §15.2's five retirement items) | two coexisting inter-process communication stacks = duplication; the decision is closed |
| four-level watermark / four channels / resume | **Build new** (L2) | apollo has none at all; the direct dependency of the attribute-sync design |
| Aeron | **Settled: not adopted**; §5.2's semantic absorption list and §5.5's filter blueprint are kept | §5.3 closed as self-developed (architecture-review §15.2) |
| FrameFilter system | **Build new** in the converged modules/net (the four-tree convergence is the precondition) | architecture-review §16.8.3-①: BW's same-library + injected-assembly precedent (udp_channel.hpp:81/:139-140) |
| L0 transport model | **reactor (epoll) as baseline**; interface fixed on "send-ring submission semantics" for completion-model compatibility — io_uring M2+ with benchmark admission, IOCP/kqueue off the roadmap (shim level) | §5.8 (2026-09-29): all three framework precedents are reactors (BW event_poller.hpp:95-137 select-only, KBE poller_epoll.cpp, skynet socket_epoll.h:20); the ipc tree's async_io.h is the repo's existing completion-model interface (outside the C-29 inventory, disposed with the ipc audit round) |
| sdshmem-style same-machine shared-memory channel | **Deferred** (settled in the same batch as the inter-process bus in P3; adopt the design, not the code — treated the same as Aeron) | carries only **same-machine bulk read-only sharing and point-to-point mirror streams** (spatial grid snapshots / monitoring metrics / G-2 hot-backup mirror streams), **never a general-purpose message bus** (architecture-review §15.2's boundary against a second coexisting IPC stands); ssengine-reference §4.4 + Aeron IPC in the same frame (§5.2) |
| client channel encryption | **OpenSSL as the single crypto source + CryptoFilter** (§5.9): X25519 + AEAD + HKDF-SHA256; MD5/SHA-1 (new code) and the rest of the forbidden table into the review red lines | libsodium's zero hits stand; sdk-contract §10.5's placeholder closed |
| outbound HTTP | **curl into vcpkg + Drogon branch deletion + synchronous APIs barred from scene threads + destination whitelist** (§5.10) | an object of §15.2's forbid-coexistence discipline; the SSRF surface |

## 7. Phased Landing

- **P1**: L1 frame format + L2 session basics (seq/ack/heartbeat/four-level watermark) + the native adapter converged as the only L0; the GameConnection Facade goes live, and attribute sync's P1 depends on this layer's attributes channel.
- **P2**: reconnect-and-resume (resume token) + the movement channel's "send newest only" + hard-watermark-coupled budget downshift with the attribute budget.
- Current status: the inter-process M1 link and Gateway egress prototype exist; Instance routing integration, multi-instance selection, Gateway draining, and production rollout validation remain in the root `todo.md`.

### Process Supervision and Recovery Boundary

Apollo's current process supervision is provided by `machined`: local process launch, death detection, and restart. It does not provide hot backups of Instance state, cross-machine takeover, or migration of active matches. The target gameplay unit is an Instance; the world is not spatially partitioned into Cell/Space processes.

When a World exits unexpectedly, its in-memory Instances do not appear automatically in another process. Until state recovery or authority transfer is implemented and verified, affected matches must be treated as failed and settled or restarted according to game rules. Moving a player between servers requires an explicit handoff of the single authoritative object and a route update; this belongs to World/Instance lifecycle work, not to the transport layer by default.

`baseappmgr`, hot-standby replicas, and automatic World rebalancing are neither required target processes nor implemented capabilities. Check `apps/machined` and the root `todo.md` for current process-supervision status.

**Inter-process state and messages:** one owner is responsible for each authoritative state; other processes receive the projections they need through messages. Transport acknowledgement, state consistency, and persistence are separate guarantees and must be designed and verified separately. Message delivery alone does not mean authority has transferred or data has persisted.

**Remote entity calls:** the current M1 link provides inter-process message transport primitives. World routing, request idempotency, authority epochs, and active-Instance migration still need implementation for gameplay that requires them. The target `OneWay` and `RequestReply` semantics in the remote-call design do not mean end-to-end cross-server gameplay is complete.

## 8. Intersections with the Other Designs

| Related design | Landing points |
|---|---|
| attribute-sync.md | the four channels are its §5.3 QoS split; the hard watermark tier triggers its token-bucket downshift; resume depends on its ViewerState.acked_seq model; frame compression likewise §7 (zstd over 256B) |
| scripting-lua.md | scripts see only the send/subscribe/callback three-piece set; async callbacks resume coroutines at tick boundaries; backpressure/reconnection invisible to scripts |
| sdk-contract.md | the contract file is the single source for L1 encoding and every end's SDK; the frame format goes into the contract (each end's plugin implements frame delimiting from it) |
| ssengine-reference.md | four points absorbed: DelaySend/GetSendBufFree/GATE/sdpkg (§5.4); the sdnet_adapter anti-example (fake namespace) is the same kind as fake-fruit |
| architecture-review.md | network adapter registration goes through core::di startup assembly (compile-time type keys), never the old string container; the adapters' form of existence = link-time selection, not runtime string switching |
| xml-generation.md | the frame header ver → filter stack declaration (16.7.1) is assembled by the contract generator (use ④); messages.xml's channel enumeration is validated by its four-layer funnel's first layer |
| logging.md | collector push = an upper-layer consumer of §5.7's InterServerLink (stable inter-process connections, independent of the client session's four channels); the Link's bounded queueing and the "collector down → write local only" degradation semantics share a source (the BW LoggerEndpoint bounded-reconnect + bounded-buffer precedent) |
| session-and-online-directory.md | the landing of §7's shared-model four-channel table's "session" row and "global arbitration state" row — the online directory = the manager domain's centralized authority + InterServerLink event projection (RouteResolver mirror data supply); §3's resume TTL = the same source value as the offline keepalive window; §5.7's authority_epoch = the displacement-competition adjudication backstop (anchor_epoch); §5.9's login_token = the entry precondition of directory registration |
| login-flow.md | §5.9's login_token ownership row (:313) and the handshake family's full-chain fulfillment — the two-stage connection (the login connection's anonymous handshake in the info="apollo-login-v1" domain separation + the game connection's ClientHello with token); §5.9 :317's three key tiers completing the credential face (the three-credential disambiguation, concept-glossary §2.5); §5.7's Admission RPC = a RequestReply low-frequency control-plane instance; §7's gates / lightest placement / displacement pre-ruling = the login admission processing order |
| inbound-interfaces.md | the **mirror face** of §5.10's three outbound rulings (curl outbound reuse / channel-domain inbound-outbound whitelists / the Drogon deletion ruling maintained without re-discussion); §5.7's InterServerLink = the callback's stage-two delivery channel (OneWay + per-Link backpressure + named endpoints); §7's G-1 fleet declares the interfaces component — a standalone process carrying inbound HTTP (gap #14, landed 2026-10-01) |

---
