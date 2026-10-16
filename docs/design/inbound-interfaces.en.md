[English](inbound-interfaces.en.md) | [中文](inbound-interfaces.md)

# Apollo Inbound Third-Party Interface Design (inbound-interfaces)

> Status: design draft (under review). Positioning: the **HTTP surface the outside world actively comes in through** — channel payment callbacks, third-party account binding callbacks, health checks — the process that hosts it, authentication, delivery, idempotency, and surface restriction. Gap registered at design-gap-inventory #14 (B11 batch, landed 2026-10-01). Cross-references: net-abstraction §5.10 (the three outbound-HTTP rulings — this design is their mirror face: outbound already ruled, inbound lives here), §5.7 (InterServerLink — the internal-domain delivery channel), §7 (baseappmgr domain/G-1); login-flow (#13, same batch — the third_party_bindings table and the channel login-mode entry); session-and-online-directory (#12 — online delivery locates via the #12 directory); attribute-sync §8.2/§8.3 (storage.xml statements / DDL discipline); scripting-lua §8 (same form of async handoff); logging §5.2 (same source of the key env discipline); sdk-contract §11.3 (internal-domain segmentation); architecture-review §25/§26 (legacy firsthand reads and the design-batch five-question input).

---

## Executive Summary

1. **Hosting = a standalone interfaces process** (same-form naming and duty as KBE `tools/server/interfaces`): a dedicated inbound HTTP interface process, declared in fleet configuration state (a G-1 component), co-resident with no game process. Of the three candidates (gateway / baseappmgr / standalone process), the comparison ruling has exactly one winner — **dual isolation of fault domain and trust domain**: a callback flood hits interfaces, not player traffic (gateway) and not the arbitration plane (baseappmgr); the public-internet exposure surface collapses to one process with no game state. BW has no such process (billing goes straight into the dbutil family), which is no counter-argument — BW's callback surface is carried by the operations-platform side, apollo carries its own.
2. **Implementation piece = the modules/net/http legacy HttpServer as the upgrade candidate (audit-gate precondition)**: http.cpp:444 `class HttpServer` (:463-473 bind/listen/acceptThread) currently has zero production consumers (only examples/http_demo.cpp:21, an internal class acknowledging itself) — §4.2 already lists the modules/net/{http,websocket} code audit as a candidate, and the upgrade (real implementation / test assets / stub boundaries) is preconditioned on that audit gate's conclusion; **Drogon is not up for re-discussion** (the §5.10 :340 deletion ruling stands — a dual-form web framework is the seed of a "fifth network stack", and the inbound interface is a thin listening layer, not a web application).
3. **Authentication = HMAC-SHA256 signature primary + IP whitelist secondary, mTLS off the roadmap**: one key per channel (channel_id dimension); the signature covers method+path+timestamp+body (time window against replay, defending against captured-traffic re-sends — a different defensive layer from login_token's one-time redemption: callback time windows are wide (minute-scale, channel retry semantics) while login tokens are narrow (60s, one-time)); the key table = fleet configuration state (same source as the key env discipline in logging §5.2 :126 — never in code, never in logs); mTLS = operational burden exceeding benefit (certificate management on the channel side is uncontrollable), listed under "explicitly not doing".
4. **Delivery = two-stage (persist → locate and deliver), idempotency key = the channel order number**: stage one writes to the DB as soon as the signature verifies (reconciliation ledger + business result tables) — "persist first, process later" is the iron law of the callback surface (channels judge success by the HTTP response; in-memory state lost = a channel retry storm); stage two queries the #12 directory to locate the baseapp (online → internal-domain message + consumed at a tick boundary; offline → already in the DB, consumed at login). The DB unique constraint (channel, order_id) plus an in-memory dedup window (recent order_id LRU) form two idempotency layers; **async never enters the scene thread** — interfaces delivers via InterServerLink and baseapp consumes at tick boundaries (same-form discipline as scripting-lua §8; callbacks never enter any tick hot path directly).
5. **Inbound surface restriction = three whitelisted classes, everything else rejected**: (1) channel callbacks (/callback/{channel} family), (2) health checks (/healthz), (3) reserved ops triggers (behind the double gate of IP whitelist + admin authentication). **Explicitly excluded**: admin UI, reports, GM operations (they go through the admin/control channels — same philosophy as the logging §5.1 exporter: the HTTP surface is only a machine-to-machine thin layer, human-machine interfaces do not live in this process); rate limiting = per-route + global + bounded queue (same source as §5.7 bounded queueing — unbounded queueing = deferring backpressure into a memory bomb).

## 0. Current State and the Gap

- **Legacy (§26 firsthand read)**: modules/net/http/http.cpp:444 `class HttpServer` (bind :463 / listen / acceptThread :463-473) — zero production consumers; examples/http_demo.cpp:21, an internal class acknowledging itself, is the repo's only reference. The five apps/ pieces (base-app/bench/gateway-app/login-app/world-app) contain no interfaces-type process. **The three hosting candidates (gateway/baseappmgr/standalone) previously had zero rulings, and the legacy HttpServer was not in the evidence list** (C-86) — this section supplies the ruling.
- **Outbound ruled, inbound blank**: net-abstraction §5.10 (:333-343) three rulings are all **outbound** (curl closure / Drogon deletion / async handoff); inbound HTTP had zero design beyond that section's closing line "the inbound surface belongs to #14". The Drogon deletions in both places (CMakeLists.txt:81-91 http, :124-133 websocket) are settled — the inbound host must not resurrect that branch.
- **Precedents**: KBE = the `tools/server/interfaces` standalone process (HTTP receives channel callbacks → queries/delivers into KBEngine, deep-dive :101); BW = no dedicated interface process (billing records go straight into dbutil/mysql_billing_system, doc 36 :266 question 11, A-grade verdict) — **the two share one trait: the interface plane is separated from game-logic processes**; apollo takes KBE's standalone-process form and adds the delivery semantics BW never worked through.

## 1. Hosting Ruling: a Standalone interfaces Process

| Candidate | Ruling | Rationale |
|---|---|---|
| **gateway-app doubling as host** | Reject | The player-traffic process must not carry the public callback face: channel callback floods / retry storms live and die with player sessions; gateway's L2 session semantics (seq/ack/four channels) are a mismatch for session-less HTTP callbacks; credentials and channel keys entering a player-facing process = a widened sensitive surface |
| **baseappmgr domain doubling as host** | Reject | baseappmgr is the arbitration single point (#12 directory critical sections / admission / recovery phase) — a callback flood hitting the arbitration plane = external traffic able to drag down global rulings; and baseappmgr's zero public exposure is a security baseline |
| **Standalone interfaces process** | **Adopt** | Fault-domain isolation (if it dies only callbacks are affected, channel retries backstop it) + trust-domain isolation (public exposure collapses to a process with no game state) + an independent capacity domain (rate limiting / scaling not coupled to the game capacity model) + KBE's twenty-year same-form precedent |

- Fleet form: G-1 component id `interfaces`, instance count declared in configuration state (horizontal scaling as channels grow — no session state, any instance can handle any callback, load balancing belongs to the fronting layer); within the P1-P2 single-process form it is embedded in the main process (the same fallback logic as login-app — in-process direct calls, zero RPC).
- **Relationship to the outbound surface**: outbound (channel API calls: order queries / fulfillment notices / binding initiation) goes through the net-abstraction §5.10 curl facility (destination whitelist includes channel domains); inbound (callbacks) arrives at this process — **one channel domain, two directions, two channel sets, one interface configuration block** (the channel table declares both the outbound endpoints and the inbound routes).

## 2. Inbound HTTP Form

- **A thin listening layer in four stages** (accept → read request → route-table dispatch → handler): no templates / no sessions / no static files — all three pillars of a web framework are unneeded; this is exactly the inbound version of the Drogon ruling's argument (§5.10 :340). The handler = signature verification (§3) → idempotency check (§4) → persist (§4) → async delivery (§4) → immediate reply to the channel (200/40x/50x semantics: verification failure 403, duplicate 200 (idempotent semantics — the channel stops retrying on success), internal persistence failure 500 (the channel retries later)).
- **Route table (configuration state)**:

| Route | Method | Auth | Rate-limit tier |
|---|---|---|---|
| /callback/{channel}/payment | POST | HMAC signature + IP whitelist | medium (channel retry-storm tier) |
| /callback/{channel}/binding | POST | HMAC signature + IP whitelist | low |
| /healthz | GET | none (intranet-restricted — listening-address-family isolation) | high |
| /ops/{action} (reserved) | POST | IP whitelist + admin token | low |

- **Listening**: production = a dedicated port + fronting layer (LB/gateway terminates TLS — interfaces speaks plain HTTP; TLS belongs to the deployment plane, not the engine, same philosophy as login-app not serving files: engine processes do not carry transport infrastructure); /healthz and admin routes listen on the intranet address family (or a dedicated port), physically isolated from the public callback routes.
- **Rate limiting**: per-route token bucket + global byte rate + **bounded queue** (overflow = 503 + channel-retry backstop — retry is built into the channel protocol; the inbound version of "never queue, never block").

## 3. Authentication

- **HMAC-SHA256 signature (primary)**: per-channel keys; the signature string = `method + "\n" + path + "\n" + timestamp + "\n" + SHA256(body)`; headers carry channel_id + timestamp + sign. The **time window** (±300s recommended by default, tuned per channel retry cadence) defends against captured-packet replay — a different layer and a different value from login_token's 60s one-time redemption (callbacks are channel machine-to-machine, and retry semantics demand a wide window; tokens belong to player clients, and single-use redemption narrows the window). Key comparison is constant-time (`CRYPTO_memcmp`, same source as the §5.9 usage discipline).
- **IP whitelist (secondary)**: a table of channel egress network ranges (configuration state), checked before signature verification — saves HMAC compute + shrinks the attack surface; an empty range table = not enabled (development).
- **Key table = fleet configuration state**: channel_id → {key, signature algorithm version, time window, outbound endpoint, IP ranges}; key values are env-injected, never in config files (same source as the logging §5.2 :126 "keys via env" discipline); rotation = a dual-key overlap window (verify against both, retire the old after the channel switches).
- **mTLS off the roadmap (explicitly not doing)**: the issuance/renewal/rotation management chain for certificates on the channel side is in the channel's hands and uncontrollable; forcing mTLS = integration friction for marginal security; HMAC + IP + time window already covers the callback-surface threat model (forgery / replay / eavesdropping — eavesdropping is handled by fronting TLS).

## 4. Callback Delivery (Two-Stage)

**Stage one: persistence (synchronous, completed inside the handler)**

```
验签通过 → 幂等检查（内存 LRU 近期 order_id + DB 唯一约束 (channel, order_id)）
        → 事务写：callback_ledger（原文 + 验签要素 + 接收时刻）+ 业务结果表（payment_orders / binding_requests 初始态）
        → 应答渠道 200
```

- **Persist-first-process-later is the iron law**: channels judge success by the HTTP response — an in-memory "process first, persist later" loses callbacks in the restart/crash window while the channel has already stopped retrying (it received a 200); keeping the raw payload in the ledger = the basis for reconciliation and dispute arbitration (the experience face of BW billing records going into mysql: the record itself is the asset, doc 36 :266).
- Idempotency key = **the channel order number** (unique on the channel side); the DB unique constraint is the authoritative idempotency (constraint conflict = duplicate callback → straight 200), the in-memory LRU saves one DB round trip on the hot path (capacity = recent order volume per channel, configuration state).

**Stage two: delivery (asynchronous, returns as soon as the handler exits)**

```
在线判定：查 #12 目录（account → baseapp 定位，#12 §5 RouteResolver 镜像同源数据）
  ├─ 在线 → internal 域消息（CallbackDelivered{type, order_ref}）→ InterServerLink → 宿主 baseapp
  │         baseapp 在 tick 边界消费（进当 tick 后续输入——battle-determinism §1 异步不进判定同律）
  └─ 离线 → 终态已在库（第二段对离线者无事可做——登录时业务逻辑读库消费，见 login-flow §4 风控/消费位）
```

- **No atomicity promise between the two stages, with reconciliation as the backstop**: stage one succeeds and stage two is lost (the process dies in between) → the ledger has the record + channel reconciliation files to cross-check (an end-of-day reconciliation task, an ops-triggered route candidate) — the callback surface's reliability model is **at-least-once + idempotent convergence**, not exactly-once (the channel protocol itself is at-least-once; exactly-once cannot be promised across two distributed sides).
- **New internal-domain messages** (sdk-contract §11 segmentation 900+, with the code batch): `CallbackDelivered` (interfaces → baseapp) + reconciliation discrepancy reports (an ops query surface, P3). RequestReply is used only for low-frequency order-status lookups (interfaces ↔ db path); delivery itself is OneWay — **callbacks never block interfaces because baseapp is slow** (per-Link backpressure §5.7; a slow baseapp drags only its own Link).

## 5. Account Binding Domain (Same Domain as #13)

- **Table = third_party_bindings** (login-flow §4, same table same columns: channel / open_id / account_id / bound_at, UNIQUE(channel, open_id) — the UNIQUE is consumed in both directions: #13's channel login looks up account, this design's binding persistence checks duplicates); DDL falls under the attribute-sync §8.3 discipline (migration scripts + statement set into storage.xml, batch 2).
- **Binding flow sequence** (client-initiated, outbound and inbound relaying):

```
1. 客户端经 login-app 发起绑定请求（auth_mode=channel 或已登录态附加绑定）
2. login-app/interfaces 出站调渠道 API（§5.10 curl，渠道域在出站白名单）→ 得渠道侧绑定票据
3. 客户端在渠道侧完成授权（渠道 App/H5——apollo 链路之外）
4. 渠道回调 /callback/{channel}/binding → 本进程验签 → 落 binding_requests
5. 在线则投递通知（第二段）；账号映射 = 新绑定建行 / 已绑定他号 → 拒绝码 channel_bound_elsewhere
```

- **Unbinding**: a symmetric flow (outbound initiation + callback confirmation + soft-delete marker — hard delete breaks the reconciliation chain); binding/unbinding audit rows go into the ledger (the same raw-payload retention discipline).
- **Responsibility boundary**: binding **business semantics** (which campaign grants what, binding rewards) belong to baseapp business (Lua consuming CallbackDelivered); binding **protocol and security** (signature verification / idempotency / persistence) belong to this design — the same dividing line as #13's "protocol belongs to design, gameplay belongs to business".

## 6. Inbound Surface Restriction (Whitelist Discipline)

- **Only three classes** (the §2 route table is the complete list — adding a route = a config change through review, not runtime registration): channel callbacks / health checks / reserved ops triggers. Any path outside the route table = 404 + a counter (a scanning-probe observation surface).
- **Explicitly-excluded list (the anti-expansion defense line)**: admin UI / report queries / GM operations / player APIs — human-machine interfaces and ops operations go through the admin/control channels (the GM command chain = the existing scripting-lua §7.5 semantics; ops observation = the logging §5.1 exporter single face). **Rationale**: interfaces' security model is "verify machine traffic by signature" — stuffing in human-machine interfaces introduces three new attack surfaces (sessions / permissions / UI), and each one violates the "thin listening layer" form; this is twenty years of boundary experience from KBE interfaces doing callback integration only.
- **No script-registrable inbound routes**: the Lua layer has zero HTTP inbound capability (the mirror sentence of logging §5.2 :126 "the Lua layer gets no HTTP outbound") — business that must respond to external events follows the existing chain of channel callback → internal message → business handler; no second entrance.

## 7. Intersections with the Other Designs

| Related design | Landing points |
|---|---|
| net-abstraction | The mirror face of §5.10's three outbound rulings (curl outbound reuse / destination whitelist includes channel domains / Drogon not resurrected); §5.7 InterServerLink = the stage-two delivery channel (OneWay + per-Link backpressure + named endpoints :231 — interfaces is a named fleet component, arbitrary delivery targets are not opened); §7 G-1 fleet declares the interfaces component |
| login-flow (#13) | third_party_bindings consumed by both from one table (#13 queries the mapping / this design writes bindings); the outbound verification stage of auth_mode=channel login sits on the relay line of the two designs; channel_rejected / channel_bound_elsewhere are the same error-code family |
| session-and-online-directory (#12) | Stage-two online determination consumes directory queries (a new consumer of this design beyond #12 §5's three consumers — read-only mirror, same source data as RouteResolver); it never writes the directory (callbacks do not change online state — displacement/registration semantics do not apply) |
| attribute-sync | §8.2 storage.xml statement set (the four tables callback_ledger/payment_orders/binding_requests/third_party_bindings); §8.3 DDL migration discipline; write-behind journal does not apply to the ledger (synchronous transactional write — callback persistence precedes the response, no behind window) |
| scripting-lua | §8 same-form async handoff (callbacks never enter the tick hot path, consumed at tick boundaries); §7.5 GM bans/kicks do not pass through this surface (an anti-expansion defense exemplar of the inbound restriction); zero Lua HTTP inbound capability |
| sdk-contract | §11.3 new internal-domain messages (the CallbackDelivered family, 900+ segmentation with the code batch); zero additions to the client domain (player clients are unaware of the callback chain) |
| logging | §5.1 exporter single face as before (/healthz is process health, not a metrics face — metrics stay on the exporter); §5.2 :126 keys via env + outbound whitelist from the same source discipline; ledger raw-payload retention and the structured-log layering (the ledger is a business asset, not a log) |
| clock-and-time | Tick-boundary consumption of stage-two delivery; the time source for time-window verification (machine-clock sync preconditions, same assumption as the #12 periodic reconciliation) |
| battle-determinism | Callbacks enter the current tick's subsequent inputs, not its rulings (the §1 same law — a deposit landing is an asynchronous external event that changes attributes) |
| capacity-and-benchmark | interfaces' independent capacity items (callback flood / retry-storm tier rate-limit parameters) enter the P3 calibration list |

## 8. Phasing and Legacy Disposition

- **Legacy (§26 firsthand read, source frozen)**: http.cpp:444 HttpServer has zero production consumers — an **upgrade candidate** (hosting the inbound thin listening layer) preconditioned on the §4.2 audit gate (the modules/net/http code surface: stub quality / test assets / C-57 event_loop errata follow-ups); if the audit concludes a rewrite is cheaper, this design's shape is unchanged and the implementation piece starts fresh (an apps/interfaces thin layer talking straight to InterServerLink) — **the design surface does not bet on legacy quality**. The examples/http_demo.cpp:21 internal-class reference is disposed with the audit gate.
- **P1 (single-process demo)**: in-process callback handling (interfaces duties direct-called in the same process, zero RPC, zero dedicated listening — use-case driven: in-memory ledger table + direct calls to business handlers).
- **P2 (Compact)**: /healthz + in-memory channel table + full-spec HMAC verification; the ledger lands in storage.xml (batch 2 dependency); delivery = same-process events (no InterServerLink — Compact's single process is naturally zero-RPC).
- **P3 (multi-process full)**: standalone process + dedicated listening + InterServerLink delivery + #12 directory online determination + outbound whitelist channel domains + reconciliation tasks (ops-triggered routes) + rate-limit parameter capacity calibration. **Acceptance interlock**: #13's channel login mode (the full binding chain) + #15 Bots (callback-storm load tests — bots simulate channel retry floods against rate limiting and the bounded queue).

---

*Baseline: apollo main @ edc94cfa (documentation state; source frozen untouched). Legacy firsthand reads = architecture-review §26 (http.cpp:444/:463-473, examples/http_demo.cpp:21, the five apps/ pieces list, the Drogon dual-deletion registration); precedents = deep-dive :101 (KBE tools/server/interfaces), doc 36 :266 (BW mysql_billing_system A-grade); outbound mirror face = net-abstraction §5.10 :333-343 firsthand read; #13 interlock = login-flow.md §4/§8 (landed the same day, same batch; line numbers are of this batch's version); #12 interlock = session-and-online-directory §5/§8. Design-batch input = architecture-review §26's five questions (this draft's five answers: hosting (1) / auth (2) / delivery (3) / binding (4) / inbound restriction (5)). Time window / LRU capacity / rate-limit tiers are initial recommendations, to be calibrated by the P3 capacity batch.*
