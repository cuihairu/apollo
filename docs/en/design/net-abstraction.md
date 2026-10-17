# Network Abstraction and Transport Kernel

This page separates current primitives from application guarantees. Business process roles are Login, Gateway, and World; Scene and Instance run inside World. Passing library tests does not establish complete gameplay, cross-server recovery, or no-downtime release.

## Executive summary

Game logic uses explicit messages, correlation, and execution boundaries. Transport delivery, business acceptance, simulation execution, and durable confirmation are separate outcomes. Inter-process traffic reuses M1 links and contract routing; Gateway owns client connections.

## 1. Current implementation

| Component | Location | Capability and boundary |
|---|---|---|
| TCP foundation | `modules/net/tcp` | Socket, reactor, and messaging primitives |
| M1 links | `modules/net/link` | Handshake, heartbeat, seq/ack, dispatch, watermarks, reconnect, and peer restart events |
| RPC | `modules/net/linkrpc` | Correlation, responses, and timeouts; not business deduplication |
| Session facade | `modules/net/gameconnection` | Four channels, priorities, delivery queues, and resume windows; application integration needs separate acceptance |
| Contract encoding | `modules/net/contract_wire` | Field encoding and decoding using generated routing metadata |
| Application encoding | `modules/protocol` | MessageCodec remains in some application paths, with M1 frame translation |
| Outbound HTTP | `modules/net/http` | CMake requires libcurl and defines `APOLLO_HAS_CURL=1`; business integration is separate |
| Client ingress | `apps/gateway-app` | TCP listener, admission, directory lookup, and World intention forwarding |

XML/XSD contracts, a generator, and `.proto` artifacts already exist. Dependencies and generated files do not establish protobuf/zstd use in real gameplay traffic.

## 2. Game-facing API

GameConnection exposes send, subscribe, state, and close. Sending distinguishes `kAccepted`, `kTrimmedLow`, and `kBackPressured`. Queue acceptance does not establish remote execution.

Applications validate identity, arguments, sessions, and player authority epochs. Clients must not receive arbitrary entity-write or server-addressing privileges. Complete instance results and state delivery still require application integration.

## 3. Layers and threads

- L0 handles transport rather than gameplay admission or rewards.
- L1 handles frames, contract encoding, and dispatch.
- L2 handles sessions, sequences, resume windows, and bounded queues.
- L3 exposes game-facing messaging.

IO and workers hand intentions and completions to the owning logic thread. WorldApp executes its bounded AcceptanceQueue at a defined gameLoop boundary. Never synchronously wait on an RPC from the thread responsible for pumping that same link.

The GameConnection inbound queue currently records dropped old entries. That library behavior cannot establish reliable business delivery. Application ingress must define overload failures and receipts for non-droppable settlement and authority operations.

## 4. QoS and backpressure

### 4.1 Four channels

| Channel | Policy |
|---|---|
| movement | Droppable; retain the latest message/state per ID |
| attributes | Reliable and ordered; declared low-priority state may be trimmed |
| events | Events requiring explicit backpressure instead of arbitrary drops |
| control | Independent small budget to avoid starvation |

Client channel policy differs from inter-process link policy. Links classify Control, Reliable, and Droppable messages; they do not arbitrarily remove queued reliable business messages.

### 4.2 Four watermarks

Default link capacity is 4 MiB, with a 16 KiB control reserve. Soft, hard, and cut thresholds are 50%, 75%, and 95%. Soft rejects new Droppable messages; cut rejects new Reliable messages. Control remains subject to its separate budget. Sustained cut closes the link after its configured limit.

Senders must handle backpressure without unbounded retries, caching, or simulation blocking. Attribute budgeting and transport admission are independent controls.

### 4.3 Inbound validation

Admission, length, rate, and argument checks precede simulation. Available primitives do not establish coverage of every application entry point. Acceptance must exercise unauthenticated traffic, forged identity, stale epochs, and overload.

## 5. Mechanism boundaries

### 5.1 Transport and business outcomes

TCP/link acknowledgments do not replace simulation receipts or durable confirmation. Sequence deduplication across reconnect does not guarantee exactly-once effects across process restart.

### 5.2 Bounded delivery

Use explicit send outcomes and bounded queues. Callers choose degradation or retry, retaining correlation and the original business idempotency key.

### 5.3 Technology direction

Current inter-process communication uses the existing M1 kernel. External designs such as Aeron can inform mechanisms without becoming a second deployed bus or an integrated dependency claim.

### 5.4 Thread handoff

Background results carry request identity to the logic boundary. Revalidate target lifetime, session, and authority before execution. Reject or cancel results arriving after object destruction.

### 5.5 Frame processing

Compression, encryption, and checks belong to frame processing and require matching connection versions. A primitive or dependency does not establish its use on every client path. Protobuf/zstd business integration remains demand-driven.

### 5.6 Evolution and capacity

Introduce optimizations behind clear layer interfaces and measured benchmarks. Completion models, shared memory, and other transports do not change authority or justify another protocol source of truth.

### 5.7 InterServerLink

Handshake includes process identity, route_version, and authority_epoch. Current route-version mismatch disconnects the link; this is not N/N-1 protocol negotiation. A peer epoch change emits a restart event for upper-layer recovery. Distinguish same-epoch reconnect from process restart.

Reliable uses send queues, receive deduplication, and cumulative ACKs. Droppable does not guarantee retransmission. The owning pump advances link state sequentially. Business transfer additionally requires durable authority records and commit reconciliation.

### 5.8 Transport models

The kernel provides TCP and testable transport interfaces. Readiness versus completion is an L0 implementation choice, not proof of platform support. Windows compilability shims do not establish real transport acceptance.

### 5.9 Client security

Authentication and authority checks belong at business ingress. Credentials and secure transport require explicit deployment integration. Prototype accounts, nonempty-ticket tolerance, and default routing are not production authentication. Configured Login admission must reject unavailable or invalid responses.

### 5.10 Outbound HTTP

The HTTP target uses libcurl, without a Drogon alternative CMake branch. Do not invoke synchronous HTTP on simulation threads. Async results enter a future logic boundary and cannot rewrite completed decisions.

Restrict destinations and timeouts. A curl_multi execution layer, cancellation, and script-coroutine handoff require real consumer integration; an existing poll loop does not establish curl_multi integration.

## 6. Active rules

Reuse M1 and contract routing, retain accurate application-frame translation boundaries, and validate client sessions, inter-process links, and business results separately. Do not infer current features from historical implementation inventories or introduce external-engine process roles into Apollo.

## 7. Fleet, recovery, and updates

machined provides local supervised restart. Active Instances do not automatically move after a World crash. Fail unverified active state explicitly while preserving confirmed profiles and settlement. Cross-machine hot standby and automatic takeover are not current guarantees.

Validate Gateway drain, World drain, cross-Gateway recovery, cross-World authority transfer, compatibility windows, and rollout rollback separately. Continued admission, no extra login, TCP continuity, and active-match continuity are distinct outcomes.

Cross-server scenes reuse [World, Scene, and Instance](./cross-server-scenes.md), without seamless maps. Implementation tasks live only in the root todo.md.

## 8. Related designs

- [Attribute synchronization](./attribute-sync.md): snapshots, deltas, visibility, and budgets.
- [Lua](./scripting-lua.md): logic-thread handoff and module replacement.
- [SDK contracts](./sdk-contract.md): encoding sources and version boundaries.
- [Gateway](./gateway-topology-survey.md): connection ownership and update acceptance.
