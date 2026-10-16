[English](gateway-topology-survey.en.md) | [中文](gateway-topology-survey.md)

# Gateway Necessity and Frontend-Connection-Layer Topology Survey (gateway-topology-survey)

> Status: research piece (2026-10-08). Positioning: answers "should apollo have a standalone gateway-app (connection-plane process)" — a duty comparison of the two topologies (with/without), nine-framework precedent trade-offs, and apollo's route recommendation. **A recommendation piece, no deciding on the user's behalf**: the landing cadence and final shape of gateway-app hang on the net M1 decision chain (improvement-plan P3-4 batch note); this piece only supplies the arguments for "whether", and rules nothing on "when/how" — the "keep" conclusion is in effect (ADR-011, 2026-10-10; landing cadence = the minimal-fix prescription, three steps first, ingress to P3-2 gateway surface). Evidence grading follows docs/36 §0: A = local source/official-doc original read this round (with path/line or URL), B = this repo's documents, C = official-doc paraphrase, D = community-dubious. Division of labor with docs/36: doc 36 is the ten-object eight-plane deep dive (A-grade local source reads); this piece takes only the "frontend connection layer vs backend RPC" axis — for the five overlapping frameworks it reuses doc 36's proven facts plus this round's fresh read citations; Pitaya/Colyseus/Nakama/Orleans are first-coverage.

---

## 0. Conclusion First

**apollo keeps a standalone gateway-app.** Backend interconnection does not copy Pomelo/Pitaya's message-middleware-everywhere RPC (nats); per the established ruling, the inter-process bus is self-developed (architecture-review §15.2 "no second IPC system" + the 2026-09-30 user ruling introducing no etcd/consul; net-abstraction §7 G-1 already on file).

The reason to keep it (benefit wording converged 2026-10-12, this one item only):

1. **Fast reconnect after disconnect** (per 火龙果 reference, user 2026-10-12) — during baseapp rebuild (§7 rebuild semantics) / scene transfer / migration the client TCP stays up; the resume semantics of the gateway row in session-and-online-directory §6 (token bound to the baseappmgr-domain session, not to a gateway instance, not to a baseapp) physically presuppose "a connection endpoint that does not live and die with the baseapp".

The remaining structural items — public-network face and risk-control single point (token verification/rate limiting/encrypted handshake centralized in one place), protocol-family closure (wss fallback belongs to the gateway), scene single-writer discipline (the baseapp frame budget is not eaten by session IO) — are demoted to **topology-structure comparison facts**, faithfully kept in the §1.1 comparison table and the §1.3 cost record, not serving as a benefit list.

Costs recorded faithfully (§1.3 counter-evidence recorded alongside): one internal forwarding hop (zero cost same-machine, one NIC round trip cross-machine) + the bandwidth share of every byte relayed through the gateway + one dedicated process's net-ring memory budget (capacity §4: per session 64KB×2×5000 ≈ 640MB magnitude) — in exchange for the one benefit above and the structural comparison face.

## 1. Gateway-Necessity Verification

### 1.1 Duty comparison of the two topologies, with/without

| Duty | With gateway (apollo's current architecture) | Direct-to-baseapp (alternative = the KBE direct-connect baseapp shape) |
|---|---|---|
| Connection retention / L2 session (seq/ack/heartbeat/resume/watermark, net-abstraction §3) | the gateway centrally holds the whole set (the established "four-channel session") | every baseapp process carries the whole set — L2 and the scene thread in one process (IO-thread/scene-thread separation still holds within the process, but the hot spots contend for cores) |
| Authentication | local token verification (login-flow §3 — the gateway holds K_login, the verification key; the account store untouched) | every baseapp holds the verification key; or the baseapp asks baseappmgr back (the incumbent gateway's misplaced-implementation precedent, ruled for migration in login-flow §10) |
| Routing | pass-through by zone_hint (established semantics; ResolveRoute asks baseappmgr for placement) | no cross-process routing needed — but a player changing baseapp (cross-process scene transfer / instance reopen) = client reconnect + full handshake + snapshot re-send (attribute-sync §11 snapshot cap 50KB) |
| Fan-out / broadcast | the gateway fans out downlink | baseapp sends directly (one hop less; under same-machine deployment fan-out is cheaper) |
| Encryption / protocol family | CryptoFilter main path + wss fallback, closed out in one place (§5.9) | every baseapp dual protocol stacks (custom TCP + wss), or cut wss and sacrifice the browser client |
| Fault domain — connection process dies | gateway dies → §6 gateway row: all entries reverse-looked-up by that gateway_id go Suspended (§4 window), the client reconnects to **any** gateway instance and resumes | no equivalent exists — the entry point is the authoritative process; authority death = entry death |
| Fault domain — scene process dies | baseapp dies → §6 baseapp-row window handling, **the client connection stays up** (the gateway holds TCP; after baseapp rebuild the Session re-attaches to the Anchor, object-model.md:129 object axis) | baseapp dies = all TCP down; the reconnect storm lands on the baseapp itself — and before §7's rebuild completes the baseapp cannot answer the resume handshake, leaving clients nowhere to land during the window |
| Scale in/out | gateway cluster adds/removes instances, clients unaware (resume not instance-bound) | baseapp add/remove = all clients on it reconnect |

The comparison table's one core difference: **in the direct topology the "entry process" and the "authoritative process" are the same one, so every rebuild/migration of the authority becomes a connection event**. The with-gateway topology splits the two — exactly the premise of §6 splitting the death rows by the deceased's role (gateway row / baseapp row); without a gateway, §6 has only one row and the recovery semantics degenerate to "client reconnect storm + the reconnect storm itself crushing the rebuilding baseapp".

### 1.2 The apollo scenario-data argument

Run both topologies through capacity-and-benchmark's established numbers:

- **Connection scale and memory**: 5000 CCU/process (capacity §1). With gateway: the gateway centralizes 5000 sessions ≈ 640MB net-ring budget (§4 table) + a dedicated IO thread — one process's capacity share. Direct: spread across N baseapps, ~5000/N sessions each — on numbers alone, direct saves "one process". But the baseapp process's deployment granularity is driven by scene load (model B's 10-scene process family), baseapp count scales with gameplay, and client-connection distribution follows the gameplay — **the planning unit of peak capacity shifts from the "connection plane" to the "gameplay plane"; the login-app peak (reconnect storm, login-flow §9 capacity item) has nowhere to be isolated**.
- **Downlink bandwidth**: whole-server egress 160MB/s saturated ≈ 1Gbps (capacity §3). With gateway: the baseapp→gateway and gateway→client segments each carry a share; cross-machine, the two segments sit on different NICs. Direct: one segment. **This is direct's real bandwidth advantage** — but under same-machine deployment (the fleet's common shape) gateway↔baseapp goes over lo/the local NIC at near-zero cost; cross-machine, the cost = one extra intranet transfer per byte, the intranet bandwidth far exceeding the public egress, so the bottleneck segment is unchanged.
- **Event amplification**: model B ~19543 broadcast events/frame ≈ 195k events/s (capacity §2.1). A fan-out of this magnitude is the same cost center whether it lands on the gateway or the baseapp (attribute-sync's per-viewer budget is already fixed in the collect/flush segment); the gateway neither amplifies nor shrinks it — pass-through, no reassembly.
- **Recovery-period landing point**: §7 semantics — the journal-less ephemeral index rebuild <1s@5000 entries, but that is the baseappmgr side; baseapp rebuild (scene reopen + Anchor replay from journal/DB) is seconds-scale. Within the window the resume handshake needs a living endpoint to answer: with gateway = the gateway answers (Suspended entries + re-attach queueing); direct = no endpoint to answer. **This item depends on no performance assumption; it is structural.**
- **Scene-thread discipline**: net-abstraction §3's thread model (IO threads carry bytes, the scene thread is single writer) holds under both topologies; the difference is core contention — direct puts the encrypt/deframe/rate-limit CPU of 5000/N sessions inside the baseapp process, sharing the CPU budget with simulate/recalc/collect (§2 frame-budget table ~55ms/100ms already allocated). Under the current stub semantics the headroom is large (model B 5.9%), but the headroom's planned use is gameplay complexity and multi-scene parallelism (capacity §2.1's conclusion direction), not session-layer hot spots.

Conclusion: **keep**. None of the four structural reasons (§0) depends on performance numbers; the performance ledger has wins and losses, a real bandwidth cost exists only cross-machine, and the recovery-period landing point plus the fault-domain split are the decisive weights.

### 1.3 Counter-evidence recorded faithfully (the direct camp is not the loser)

Among the nine frameworks the direct/unified camp is the majority (BW/KBE/TrinityCore/Nakama/Colyseus/Orleans, §2); the precedent count itself is symmetric evidence, so why apollo stands on the minority side must be stated:

- **BW/KBE's baseapp is "entry + entity host" unified** — the proxy entity resides on the baseapp, the client channel lands on it anyway, and the cellapp that the authority was split to never sees the client. Unified is not their compromise; it is their essential shape. After apollo split entity authority to the baseapp (scene single writer + Home baseapp holding PlayerAnchor, object-model.md:129), entry and authority are naturally two things — **the gateway is not "an extra process"; it is the other half of the split**.
- **Orleans officially recommends co-hosting, gateway-free** (learn official client page read this round: co-hosted client "doesn't need a separate gateway. This avoids a network hop and a serialization/deserialization round trip") — but the same page gives the precondition: "Client code is often very *thin* (e.g., translating incoming HTTP requests into grain calls)". A game client connection is not thin forwarding: L2 four channels + resume + watermark + rate limiting is a full stateful session layer; the thin argument does not hold. Orleans's external-client shape (frontend web servers acting as "connector or conduit") says exactly the opposite: **when the connection plane is heavy, it becomes a dedicated thing**.
- **skynet proves "a forced topology can be absent"** — gate is an optional service; a single node can skip it entirely. That is the positioning difference between library and framework (skynet gives primitives, not topology), not a refutation of apollo's topology; apollo's four-tier targets (light online / room-based / ordinary MMO / BigWorld enhanced state, kbe-reference-principles §2) require the topology to have conventions, not that every tier look the same.

### 1.4 Conclusion and role narrowing

Keep gateway-app, with the role narrowed per the existing design — only the connection-plane three: token verification, four-channel session carrying, zone_hint pass-through; no admission (login-flow §1 already ruled: admission to baseappmgr, authentication to login-app), no business logic, no file service (contract-package pointer scheme, login-flow §6). The thinner the gateway, the more harmless it is as the direct camp's "one less hop" argument in the §1.1 table gains force — a thin pass-through hop is forwarding cost, not logic cost.

## 2. Nine-Framework Comparison: Frontend Connection Layer vs Backend RPC

### 2.1 Master table

| Framework | Language/lineage | Frontend connection layer | Backend interconnect/RPC | Service discovery | Evidence grade |
|---|---|---|---|---|---|
| BigWorld | C++ (open-source 14.4.1) | **no standalone gateway** — loginapp short connection assigns a baseapp, the client connects directly to baseapp (entry + proxy-entity host unified) | self-developed Mercury (TCP/UDP dual mode + two filter families + windowed retransmission) | bwmachined daemon + machine_guard protocol | A (local source, docs/36 §2.1 + this round's server directory listing) |
| KBEngine | C++ | loginapp (standalone login process, incl. SDK delivery) → baseapp (entry + entity host unified) | self-developed bundle/channel + EntityCall | machine process UDP broadcast reply | A (loginapp.cpp:1185-1186 read this round + docs/36 §2.2) |
| skynet | C + Lua | gate service (optional, self-assembled topology) | cluster.call (inter-node RPC, node table manually configured) | no auto-discovery (cluster.conf manual) | A (service/gate.lua, lualib/skynet/cluster.lua read this round) |
| Pomelo | Node.js (2012–2023.09 archived) | **dedicated connector frontend** (hybridconnector/sioconnector) | self-developed RPC: proxy + remote + mailbox/mail station + master events | master server (subscribing to server changing events) | C (official wiki read this round) |
| Pitaya | Go (Pomelo-lineage successor) | **dedicated connector frontend** (frontend) | **nats as primary RPC transport** (cluster example), plus a gRPC shape (cluster_grpc example) | etcd | C (official README read this round) |
| TrinityCore | C++ (WoW emulator) | **no gateway** — authserver (login) + worldserver (game direct) two processes | no backend interconnect (single-world monolith) | none | A (3.3.5 source tree read this round) |
| Colyseus | Node.js/TS | **room process is frontend+backend unified**; after matchmaking, direct connect to the host process; NGINX dynamic port proxying | no inter-process RPC (rooms do not communicate) | Redis (RedisPresence + RedisDriver, matchmaking registry) | C (official scalability page read this round) |
| Nakama | Go | **no standalone gateway** — monolithic server terminates sockets (WebSockets/rUDP) + management plane gRPC/HTTP | subsystems within the monolith; enterprise cluster gossip+CRDT + cross-node message routing | built-in (enterprise) | C (official architecture page read this round) |
| Orleans | .NET | **no dedicated gateway process** — external clients connect to silo endpoints (multi-gateway list refreshed per the membership table); official recommendation of co-hosting avoids the gateway entirely | self-developed inter-silo messaging (TCP + grain directory) | membership table | A (official client page + GatewayOptions read this round) |

### 2.2 Per framework: shape / why / fits / does not fit

**BigWorld** (grade A, proven in docs/36 §2.1 + this round's `programming/bigworld/server/` directory-listing recheck)

- Shape: loginapp does only login and assignment (baseappmgr `minAppLoad()` picks the lightest — baseappmgr.cpp:588-599, loginapp :1117 consumes the (addr, load) routing, proven in docs/36); the client thereafter connects directly to baseapp; the server directory family loginapp/baseapp/cellapp/baseappmgr/cellappmgr/dbapp(mgr)/reviver, no gateway process.
- Why: the baseapp is already the proxy-entity host and the client-channel endpoint — entry and authority unified; splitting would add a hop; Mercury gives all processes one network semantics (net-abstraction §5.7 "server code uses Mercury across the board").
- Fits: seamless big-world MMO (seamless cell migration hands "the connection follows the entity" entirely to the baseapp proxy segment to digest).
- Does not fit: shapes where the entry process must scale or fail independently — a baseapp restart = all clients on it reconnect (BW mitigates with hot standby/takeover, the full G-2 precedent set; apollo's P3 skeleton takes two pieces).

**KBEngine** (grade A)

- Shape: loginapp standalone login process (authentication + clientsdk_downloader SDK delivery, cited in login-flow §0); on login success the baseapp address is sent to the client — `loginapp.cpp:1185-1186` (read this round): 「客户端得到baseapp地址的同时也会返回这个账号名称 / 客户端登陆baseapp应该使用这个账号名称登陆」 (the client receives the baseapp address and also this account name; the client should log into baseapp with this account name); thereafter the client connects directly to baseapp, cellapp never sees the client (the baseapp directory holds the entity/forwarder/backuper/data_download full set — proxy + persistence + fan-out unified, this round's directory listing).
- Why: entry-authority unified, isomorphic with BW (KBE is the BW lineage simplified); the standalone loginapp only solves "account domain separated from game domain" (apollo login-flow, same ruling).
- Fits: mid-scale MMO; KBE has no process-level disaster recovery (architecture-review C-51) — under the direct shape, connection recovery is entirely up to the client.
- Does not fit: shapes needing connection-plane-independent recovery semantics (KBE has no counterpart of §6's gateway row; apollo's §6 two-row semantics is an increment the KBE direct shape does not have).

**skynet** (grade A)

- Shape: gate is an optional service (`service/gate.lua` read this round: `connection = {} -- fd -> connection` table + `skynet.register_protocol { name = "client", id = PTYPE_CLIENT }` — gate holds connections, delivers client messages by PTYPE_CLIENT to agent services); inter-node cluster.call (`lualib/skynet/cluster.lua` read this round: clusterd per-node sender); node table manual, no auto-discovery.
- Why: skynet is a library, not a framework — it gives primitives only (gate/cluster/watchdog); the user assembles the topology; a single node can skip gate entirely, cluster appears only multi-node.
- Fits: teams able to assemble their own topology; card/SLG/casual mid-low connection densities; skynet's debug_console/monitor is apollo's P3 observation-tool precedent (cited in net-abstraction §7).
- Does not fit: teams wanting out-of-the-box fleet/discovery/recovery semantics — machined-style supervision, death notification, recovery phases all self-grown (apollo's G-1 taking the BW daemon precedent fills exactly this).

**Pomelo** (grade C, official wiki read this round)

- Shape: dedicated connector frontend + backend server pool + master management process. RPC self-developed, three layers (official wiki "RPC-Framework" page original, read this round): proxy component ("builds client stubs and mounts them on `app.rpc`") + remote component (server-side loading); mailbox ("a mail box corresponds to a remote server", lazy connection — "establish the connection to remote server when the client first initiates an RPC invocation") + mail station ("maintains all the mail box instances for current server", forwarding by id); proxy subscribes to master at startup ("subscribe to master server for server changing events"). Connector customization surface (official wiki "Understanding-Connector" page, read this round): hybridconnector/sioconnector two implementations, the listen/kick/encode/decode contract.
- Why: Node.js single-threaded — "holding tens of thousands of connections" and "running room logic" are two event-loop loads that do not fit one process; multi-process is inevitable and the frontend must be dedicated.
- Self-positioning (README, read this round): "mobile, social, web, MMO rpg(middle size)" — middle size is the official self-limit.
- Lesson note: archived 2023-09-25 (repo banner, read this round). Frontend/backend split + self-developed RPC + master single point is an ops burden for small-mid teams; the successor Pitaya switched to Go + external middleware (§2.3).

**TrinityCore** (grade A)

- Shape: two processes — authserver (login/account) + worldserver (game world, client direct); `src/server/{authserver, worldserver, database, game, scripts, shared}` (3.3.5 source tree, read this round); no backend interconnect — single-world monolith, in-process packet handling.
- Why: emulator tradition — the world server is a single-process authority; login and world are separate processes only because the protocols differ (SRP6 vs game packets), not a topology split.
- Fits: single-world MMO emulators (hundreds-thousands CCU/world); multiple realms = multiple complete process sets (authserver shareable).
- Does not fit: multi-process horizontal scaling — TC has no "one world split across processes" shape; scaling out = duplicating the whole set.

**Colyseus** (grade C, official docs read this round)

- Shape: the room process is frontend+backend unified — each Node process runs N rooms; after matchmaking takes a seat reservation the client connects directly to the host process; cross-process scaling via Redis (official scalability page original: "Redis is required for scaling"; RedisPresence + RedisDriver pub/sub); NGINX dynamic port proxying (`location` regex takes the port number → `proxy_pass http://127.0.0.1:$PORT`) or Traefik service discovery.
- Why: the extreme shape of the room model — the room is the unit of running and isolation, rooms do not communicate, so the inter-process RPC problem simply does not exist; the proxy and Redis only solve "finding the room's process".
- Fits: casual/turn-based/small-room realtime (official repo self-description "realtime and turn-based games, matchmaking", read this round); small per-room populations, strong inter-room isolation.
- Does not fit: cross-room interaction (world chat/lobby/cross-room gameplay must be self-built), large-AOI high-frequency sync (Node single-threaded + no per-viewer budget system).

**Nakama** (grade C, official docs read this round)

- Shape: monolith — official architecture page original: "a monolithic stateful server that exposes real-time and non-real-time APIs from multiple subsystems"; socket plane "runs on WebSockets and rUDP" (protocol buffers or JSON), management plane "runs on gRPC and HTTP"; runtime modules (Go/Lua/TypeScript) run the authoritative match handler in-process; enterprise cluster gossip + CRDT service discovery + "tracks the whole cluster's set of socket connections to clients and routes incoming messages to the right nodes".
- Why: BaaS shape — auth/storage/leaderboards/chat are generic subsystems; single-binary ops is simplest; realtime battle is embedded as a subsystem, not an independent topology layer.
- Fits: small-mid general backends (casual/social/turn-based) + mid-low frequency realtime; no topology assembly exposed outward.
- Does not fit: frame-level realtime scene sync — single-threaded per-match handler, no AOI/per-viewer system, message routing by node rather than by session channel (misaligned with four-channel QoS needs).

**Orleans** (grade A, official docs read this round)

- Shape: grain virtual-actor cluster; external clients connect to silo endpoints — GatewayOptions's "list of active gateways" ("The list of active gateways will be refreshed every minute by default") refreshed per the membership table, the client picks one among the multi-gateway list; officially **recommends co-hosting** (client in the same process as the silo): "doesn't need a separate gateway. This avoids a network hop and a serialization/deserialization round trip"; the external-client shape = frontend web servers that "acts as a connector or conduit to the cluster".
- Why: cloud-native design — the connection plane is assumed thin (HTTP requests translated into grain calls), all game logic in grains; the gateway degenerates from "dedicated role" to "an endpoint role of any silo" (the Orleans 1.x dedicated gateway node is a historical shape — no quotable original obtained this round, C/D-grade evolution note, not used as an argument).
- Fits: turn-based/strategy/lobby services — grain-granularity state, request-response dominant, tolerant of observer best-effort one-way notification.
- Does not fit: high-frequency downlink broadcast streams — observers "sent as one-way, best-effort messages" (official original), no QoS channels/backpressure/watermark system; game session-layer needs misalign with grain-call semantics.

### 2.3 Pitaya close-up (the named item): the trade-off of full-RPC backend

**Architecture facts** (official README, read this round): Go; connector as frontend, room as backend (official example "a frontend connector and a backend room"); service discovery etcd ("(optional, used for service discovery)"); RPC over nats ("(optional, used for sending and receiving rpc)"), plus a gRPC shape (the cluster_grpc example alongside — **the README never uses the word "default"; "nats as primary transport" follows the official example organization and common community phrasing, a C-grade faithful note**); lineage: credits pomelo for "inspiration on the distributed design and protocol" and nano as "the framework pitaya is based on"; self-positioning "an simple, fast and lightweight game server framework with clustering support… for distributed multiplayer games and server-side applications".

**The trade-off of a full-RPC backend** (this round's analysis, marked as analysis, not citation):

- Gains:
  1. Zero connection state on backend processes — arbitrary scaling, rolling releases without client disconnects; session state stays on the connector or is serializable to the backend; small failure radius;
  2. Thin frontend — the connector is pure forwarding and reusable; business iteration never touches the connection layer;
  3. Inter-service calls unified to RPC semantics (including server-to-server); the business is topology-unaware; "who is where" turns from a routing problem into an etcd query;
  4. nats hands you fanout/subscription semantics for free — broadcast-type messages (chat/matchmaking notifications) are carried by the middleware.
- Costs:
  1. Every client message takes two-plus hops (client→connector→nats→backend→back the same way), serialized at each segment — the CPU/latency tax is heaviest on high-frequency small frames like movement;
  2. Two external pieces to deploy/monitor/fail (nats + etcd) — a significant ops-plane expansion for small-mid teams (the mirror of the Pomelo archive lesson: a different set of middleware, the ops plane not eliminated);
  3. **The semantic misalignment is the fundamental one**: message-middleware delivery semantics (subject granularity, at-least-once, no per-session seq/ack/watermark) and game-session semantics (per-channel reliability tiers, resume continuation, backpressure) are different layers — the four-channel set still has to be self-grown; nats solves transport, not sessions;
  4. Thin recovery semantics: a connector restart = all its connections rebuilt; what etcd senses is services going up/down, not "how sessions recover".
- Fits: the room/lobby/chat/matchmaking plane of room-based/casual/social/mid-scale MMO — mid-low message frequency (per-person messages/second scale), session state droppable and rebuildable, intra-hop latency tolerance at the 10ms scale; the TFG lineage is exactly mobile social battle (same origin as Pomelo's self-described middle size).
- Does not fit: AOI-dense broadcast (the magnitude of apollo model B's 195k events/s/process — neither throughput nor semantics fit relaying via nats), deterministic lockstep (battle-determinism domain), seamless-world entity migration.
- **Against apollo's discipline**: apollo's 2026-09-30 user ruling introduces no etcd/consul (small game-server topology, low-frequency change; machined daemon + UDP broadcast, eventually consistent, suffices — filed in net-abstraction §7 G-1); nats the same ruling — the inter-process bus is self-developed (architecture-review §15.2 forbids a second IPC system; §5.6's M1 evolution ladder). Pitaya's external-piece cost is exactly the empirical mirror of the ruling's rationale: it traded the complexity of "self-developed RPC" for the complexity of "middleware ops"; apollo's ruling is the third road — a self-developed bus whose semantics follow §5.2's absorption list (borrowing from Aeron/Mercury), introducing no external piece and re-inventing no message queue.

## 3. Synthesis: Three Frontend-Connection-Layer Forms × Two Backend-Interconnect Families

**Three forms of the frontend connection layer**:

| Form | Members | Essence |
|---|---|---|
| ① dedicated frontend process (split camp) | Pomelo connector, Pitaya connector, apollo gateway-app | connection plane and authority plane split — authority rebuild/migration does not tear the connection |
| ② entry + authority unified (unified camp) | BW baseapp, KBE baseapp, Nakama monolith, Colyseus room | the authoritative process doubles as the connection plane — one hop less; the cost is authority events = connection events |
| ③ no frontend concept (minimalist camp) | TrinityCore worldserver, skynet self-assembly | topology left to the user / monolith all the way |

(Orleans straddles ②③: the co-host recommendation = ②; the external client's multi-gateway list = a weakened form of ①.)

**One regularity**: the frontend form correlates strongly with "whether the authority was split" — in BW/KBE the authority is on baseapp, so the entry is on baseapp; in apollo the authority is on baseapp (scene single writer), so the entry is independent; in Colyseus/Nakama the authority is the process, so there is no second process to split. **The precedent did not choose the topology; the authority layout determined the topology** — which is also the bottom logic of the §1.4 role-narrowing argument.

**Two families of backend interconnect**:

| Family | Members | Criterion |
|---|---|---|
| Self-developed RPC | BW Mercury, KBE, skynet cluster, Pomelo, Orleans | the game-server mainstream — needs mailbox/EntityCall semantics and hot-path overhead control |
| Message middleware | Pitaya (nats) | the generic-distributed trade-off — outsourcing discovery/subscription/fanout at the cost of semantic misalignment and an ops plane |

gRPC-class appears only off the hot path (Nakama management plane, Pitaya's optional shape) — on the hot path it is without exception self-developed or middleware; there is no precedent of gRPC going straight into a game hot path.

## 4. Which Road apollo Takes + Reasons (a recommendation piece, no deciding on the user's behalf)

**Take: standalone gateway-app (connection plane) + baseapp never sees the client + self-developed inter-process bus (net M1) + login-app standalone short-lived login connection (ruled in login-flow).**

- **Why ① the split camp and not ② the unified camp**: apollo's authority layout is already fixed (object-model.md:129 object axis Connection (gateway) → Session (losable, re-attachable) → PlayerAnchor (Home baseapp, never moves)) — the Session's owner is the gateway; the Anchor's migration semantics (no migration) and scene-transfer destroy-and-rebuild both presuppose "connection endpoint ≠ authority endpoint". Follow the §1.1 table: §6's two death-row semantics, §4's window, resume re-attach — the whole session-recovery design has nowhere to live in the direct shape. Of the four-tier targets (light online / room-based / ordinary MMO / BigWorld enhanced state), the light-online/room tiers can co-locate gateway/baseapp on one machine (same-machine one-hop cost near zero, §1.2), without giving up the ordinary-MMO tier's recovery semantics for them.
- **Why the backend takes no middleware family**: analyzed in §2.3 — semantic misalignment (four channels/resume/watermark still self-grown) + external-piece ops plane + hot-path serialization tax; apollo's ruling chain (§15.2 forbids coexistence + 2026-09-30 no etcd/consul + §5.6 M1 self-developed bus) sits on the same side as the nine-framework mainstream (six self-developed RPC).
- **Same form, different cause, versus Pomelo/Pitaya connectors**: the topology shape is the same (dedicated frontend), the reasons differ — they split out of Node single-threaded load necessity; apollo splits for scene single-writer discipline and recovery semantics. Same form, different cause means what can be learned is the form, not their implementation motive.
- **The mirror with Orleans**: Orleans teaches "if the connection plane is thin enough to co-host, don't split"; apollo's connection plane is not thin (full session layer), so it splits. The two don't contradict; the criterion is one and the same: whether the connection plane is thin.

**Decision boundary (decided 2026-10-10, see ADR-011)**: gateway-app today is a half-finished shell — rearchitecture/architecture.md:15 ("skeleton complete; client ingress unwired (NullClientIngressServer idling); sendToClient placeholder; resolveRoute placement-port mismatch 9002 vs 9003") + improvement-plan.md:172's reconnaissance three-cause batch note (startup hard dependency on all backends / default port misalignment / the ChatApp ghost dependency). This piece only supplies the arguments for "whether a standalone gateway" — the "keep" conclusion is in effect (net M1 decision chain, settled in ADR-011: this batch only closes the three standing causes; ingress wiring goes to P3-2 gateway surface with net M1).

## 5. Intersections with the Other Documents

| Related document | Landing |
|---|---|
| docs/36-MMO_Frameworks_Comparative_Analysis.md | the master source of the five frameworks' (BigWorld/KBEngine/skynet/Pomelo/WoW-TC) A/B/C-grade evidence — this piece reuses its §2.1/§2.2/§2.4/§2.9 proven facts; this piece's increments = the frontend-connection axis + the four first-coverage frameworks Pitaya/Colyseus/Nakama/Orleans |
| net-abstraction | §5.4 GATE variant → gateway-app; §5.9 wss fallback settles with the gateway-app decision (reason 3); §5.6 M1 inter-process bus (backend-interconnect ruling); §5.7 InterServerLink = the baseapp↔gateway link foundation |
| login-flow | §1 two-phase connection / the rejected counter-proposal of login-through-gateway on record; §3 token verification to the gateway (role narrowing); §10 the incumbent session_admission_service RPC ask-back misplacement |
| session-and-online-directory | §6 gateway row / baseapp row — the semantic body of reason 1; §4 disconnect window |
| incumbent gateway semantics | token verification, four-channel session, zone_hint pass-through — the incumbent definitions behind the role narrowing |
| object-model | :129 process axis / object axis (Session owner = gateway, PlayerAnchor does not migrate) — the basis of "authority layout determines topology" (§3 regularity) |
| capacity-and-benchmark | §1/§2.1/§3/§4 supply the numbers (5000 CCU / 1Gbps / 195k events/s / 640MB ring budget) |
| improvement-plan | P3-4 batch note's gateway three-cause reconnaissance (the source of the half-finished status); P3-2 gateway surface (the ingress wiring hook point) |

## 6. Evidence Source List (read this round, 2026-10-08)

**Local source (grade A)**:

- KBEngine: `~/workspaces/kbengine/kbe/src/server/loginapp/loginapp.cpp:1185-1186` (the baseapp-address delivery comment), `kbe/src/server/baseapp/` directory listing (the entity/forwarder/backuper/data_download unified face)
- skynet: `~/workspaces/skynet/service/gate.lua` (fd→connection table + PTYPE_CLIENT registration), `~/workspaces/skynet/lualib/skynet/cluster.lua` (clusterd sender)
- BigWorld: `~/workspaces/BigWorld/programming/bigworld/server/` directory listing (loginapp/baseapp/cellapp/mgr family/reviver, no gateway); line-level evidence reused from docs/36 §2.1 (baseappmgr.cpp:588-599/:1117 etc.)

**Official docs (grade C, all URLs read this round)**:

- Pitaya README: https://raw.githubusercontent.com/topfreegames/pitaya/master/README.md
- Pomelo repo (archive banner 2023-09-25) / wiki (RPC-Framework, Understanding-Connector, the Frontend Server page organization): https://github.com/NetEase/pomelo
- TrinityCore 3.3.5 source tree: https://github.com/TrinityCore/TrinityCore/tree/3.3.5/src/server
- Colyseus scalability: https://docs.colyseus.io/scalability
- Nakama architecture: https://heroiclabs.com/docs/nakama/getting-started/architecture/
- Orleans clients: https://learn.microsoft.com/en-us/dotnet/orleans/host/client ; GatewayOptions: https://learn.microsoft.com/en-us/dotnet/api/orleans.configuration.gatewayoptions and its source GatewayOptions.cs (dotnet/orleans)

**Reused from this repo (grade B)**: docs/36, net-abstraction, login-flow, session-and-online-directory, object-model, capacity-and-benchmark, architecture-review §15.2/§25, improvement-plan P3-2/P3-4 (citations in the body).

---

*Baseline: apollo main @ 8a2c919a. External documents and local source were all read this round, 2026-10-08; "nats as primary transport" follows the official example organization's wording (the README never uses the word default — a C-grade faithful note); the Orleans 1.x dedicated gateway node is a historical account (no quotable original obtained this round, C/D grade, an evolution note only, not an argument). The §2.3/§3 trade-off analysis and regularity synthesis are this piece's own analysis, marked as non-citation. This is a recommendation piece — the gateway decision belongs to the net M1 chain (improvement-plan P3-4); no deciding on the user's behalf.*
