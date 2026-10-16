---
title: RemoteEntityCall Design
icon: arrows-turn-right
order: 21
category:
  - Architecture
  - Apollo
tag:
  - Apollo
  - RemoteEntityCall
  - Message
  - RPC
---

[English](remote-entity-call-design.en.md) | [中文](remote-entity-call-design.md)

# RemoteEntityCall Design

This document settles a problem Apollo cannot get around when it later builds multi-process worlds, player anchors, and cross-node entity collaboration:

`How exactly do entities make remote calls to each other, and what should internal messages look like.`

If this layer is not filled in, Apollo will easily stay stuck at:

- Sending raw messages directly by `service name + channel`
- Message contents agreed ad hoc by each caller
- entity route, authority, and version info scattered across the business layer

It runs in the early days, but it will definitely turn into a mess later.

## 1. Conclusion First

On top of the existing `Channel`, Apollo needs to add an explicit remote-entity-call model:

- `RemoteEntityRef`
- `RemoteMethodSchema`
- `InternalMessageEnvelope`
- `RouteResolver`
- `EntityCallEncoder`
- `RemoteEntityCall`

The goal of this layer is not to build yet another heavyweight RPC framework, but to close the call boundaries between `PlayerAnchor`, `AvatarEntity`, `GhostReplica`, and `Witness`.

## 2. Direct Evidence in KBE

From the local source:

- [entity_call.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/entity_call.h)
  - `EntityCall` is an explicit remote entity reference
  - `createRemoteMethod`
  - `newCall`
- [entitycallabstract.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/entitydef/entitycallabstract.h)
  - Holds `componentID`
  - Holds `ENTITYCALL_TYPE`
  - Holds `ENTITY_ID`
  - `sendCall`
  - `getChannel`
- [bundle.h](/C:/Users/cui/Workspaces/kbengine/kbe/src/lib/network/bundle.h)
  - `Bundle` writes packets along message boundaries
  - `newMessage`
  - `finiMessage`
  - Supports sequential multi-field encoding

This shows that the key point of KBE is not "being able to send data across processes," but:

- Remote entity references are explicit objects
- The call method and routing info are bound to this reference
- The packet boundary is not the business layer improvising string concatenation

## 3. Apollo's Current Gaps

From the current code:

- [channel.hpp](/C:/Users/cui/Workspaces/apollo/modules/net/protocol/include/apollo/net/protocol/channel.hpp)
  - `Channel` only provides `connect/send/receive`
  - It faces endpoints, not entity routes
- [endpoint.hpp](/C:/Users/cui/Workspaces/apollo/modules/net/protocol/include/apollo/net/protocol/endpoint.hpp)
  - `Endpoint` leans toward a service-discovery result
  - No entity-level routing semantics
- [gateway_server.cpp](/C:/Users/cui/Workspaces/apollo/apps/gateway-app/src/gateway_server.cpp)
  - Current forwarding leans toward "routing by app/service"

This means what Apollo lacks right now is not sockets, but the following three layers:

### 1. Missing explicit remote entity references

Currently there is no unified object expressing:

- Which entity I want to call
- Whether the target is an anchor, an avatar, or a ghost
- What the current version of this route is

### 2. Missing a unified internal message envelope

If messages keep being assembled by each caller, they will lack:

- trace
- route version
- authority role
- method alias
- payload codec

### 3. Missing a schema-driven method table

Apollo already defined the `MethodSchema` direction in the original EntitySchema design (deleted, traceable in git), but has not yet turned it into "an executable remote-call table."

## 4. Recommended Target

Apollo should unify remote calls into this chain:

```text
Business Object
    -> RemoteEntityRef
    -> RouteResolver
    -> InternalMessageEnvelope
    -> EntityCallEncoder
    -> Channel
```

That is:

- Business code only declares "call whom, call which method, with what arguments"
- Routing, versioning, encoding, and the packet boundary are handled by the unified layer

## 5. Core Object Design

### `RemoteEntityRef`

This is the remote entity reference object.

It should contain at least:

```text
RemoteEntityRef
    entity_id
    entity_type
    target_domain
    target_app
    authority_role
    route_version
    shard_key
```

Where:

- `target_domain`
  - `Anchor`
  - `World`
  - `Proxy`
  - `Ghost`
- `authority_role`
  - `Authoritative`
  - `Replica`
  - `ForwardOnly`

### `RemoteMethodSchema`

On top of `EntitySchema`, add a layer of executable method definitions:

```text
RemoteMethodSchema
    method_name
    method_alias
    invoke_mode
    target_domain
    arg_types
    codec
    timeout_ms
    idempotent
```

`invoke_mode` should support at least:

- `OneWay`
- `RequestReply`
- `ReliableEvent`

### `InternalMessageEnvelope`

This layer is the unified message header, Apollo's version of `Bundle`.

It should contain at least:

```text
InternalMessageEnvelope
    message_type
    trace_id
    request_id
    source_app
    target_app
    entity_id
    method_alias
    route_version
    authority_epoch
    flags
    payload
```

### `RouteResolver`

Responsibilities:

- Find the current real host from a `RemoteEntityRef`
- Check whether the route version has gone stale
- Decide whether to forward to `PlayerAnchor`, `WorldHost`, or `GhostRoute`

### `EntityCallEncoder`

Responsibilities:

- Encode arguments according to `RemoteMethodSchema`
- Write into the envelope
- Perform alias compression
- Output `apollo::net::protocol::Message`

## 6. Recommended Message Categories

Do not mix all internal messages into one kind of "generic RPC."

At least 4 categories are recommended:

### 1. `EntityMethodCall`

Used for:

- `PlayerAnchor` calling `AvatarEntity`
- `AvatarEntity` calling back `PlayerAnchor`
- `Proxy` notifying `Witness`

### 2. `RouteControl`

Used for:

- Route refresh
- Authority epoch updates
- Ghost forwarding switches

### 3. `LifecycleEvent`

Used for:

- Entity creation
- Entering world
- Leaving world
- Migration completion

### 4. `ReplicationCommand`

Used for:

- Requesting incremental snapshots
- Acking a replication window
- Detail level adjustments

This way, when Apollo troubleshoots problems later, logs and monitoring are both much easier to close in on.

## 7. Recommended Call Flows

### Standard one-way call

1. Business obtains a `RemoteEntityRef`
2. Find the target method via `MethodSchema`
3. `RouteResolver` locates the target app and route version
4. `EntityCallEncoder` encodes the envelope
5. Send through `Channel`
6. The receiving side dispatches by `entity_id + method_alias`

### Request-reply call

1. Generate a `request_id`
2. Send the `InternalMessageEnvelope`
3. The receiving side executes and replies
4. The caller closes out the promise/future by `request_id`

### Stale route handling

1. The receiving side finds `route_version` outdated
2. Return `RouteStale`
3. The caller refreshes the `RemoteEntityRef`
4. Resend

This step must be done explicitly; never rely on the business layer to guess.

## 8. Relationship with the Object Model

This layer must align with the [Base Cell Proxy Object Model](./base-cell-proxy-model.en.md).

### `PlayerAnchor`

Well suited to carry:

- Long-term online state
- World assignment
- Reconnection recovery

Therefore it is the more suitable stable entry point for many remote calls.

### `AvatarEntity`

Well suited to carry:

- Real-time in-space behavior
- AOI
- Combat presentation

Its route changes more easily, so it depends more on `route_version`.

### `Proxy`

Well suited to carry:

- Sending toward the client
- Session binding

But it should not become the default entry point for all business calls.

## 9. Recommended Apollo Interface Sketch

It is recommended that later module-layer work shape interfaces roughly like this:

```cpp
struct RemoteInvokeOptions {
    InvokeMode mode;
    std::chrono::milliseconds timeout;
    bool allowRouteRefresh;
};

class RemoteEntityCall {
public:
    explicit RemoteEntityCall(RemoteEntityRef ref);

    bool invokeOneWay(std::string_view method, const EncodedArgs& args);
    Expected<EncodedReply, InvokeError> invokeRequest(
        std::string_view method,
        const EncodedArgs& args,
        RemoteInvokeOptions options);
};
```

The key point here is not what the interface looks like, but that:

- route
- schema
- envelope
- trace

must be uniformly sealed in.

## 10. Current Landing Recommendation

Apollo does not need to build the full KBE-style scripted `EntityCall` in one go right now, but should at least fill in these 4 steps first:

### Step 1: Add the envelope

Define the unified internal message header first; stop sending raw string payloads.

### Step 2: Add `RemoteEntityRef`

Extract the common remote reference out of `PlayerAnchorRef`, `AvatarRef`, and `ProxyRef`.

### Step 3: Add `RouteResolver`

Give route refresh, migration redirection, and ghost forwarding a unified entry point.

### Step 4: Add the schema-driven method dispatch table

Do not let method names and argument formats keep scattering across business code.

## 11. Landed Implementation (2026-10-13)

The `modules/net/remoteentity/` submodule has delivered the four-piece core:

| Piece | Files | Status |
|---|---|---|
| `RemoteEntityRef` | `include/apollo/net/remote/entity_ref.hpp` | ✅ Landed |
| `RemoteMethodSchema` | `include/apollo/net/remote/method_schema.hpp` | ✅ Landed |
| `RouteResolver` | `include/apollo/net/remote/route_resolver.hpp` + `src/route_resolver.cpp` | ✅ Landed |
| `RpcTrace` | `include/apollo/net/remote/rpc_trace.hpp` + `src/rpc_trace.cpp` | ✅ Landed |

**Module build**: `modules/net/remoteentity/CMakeLists.txt` — STATIC library + ALIAS `apollo::net_remoteentity`, links `apollo::net_linktrace` + `apollo::core`.

**Tests**: `tests/test_remote_entity.cpp` — 14 cases (RemoteEntityRef 2 + RemoteMethodSchema 2 + RouteResolver 5 + RpcTrace 5), all green in the build tree + sanitizer tree.

**commit**: `49d3d3a8` — `feat: add remoteentity module with trace integration`

**Boundaries**:
- The `InternalMessageEnvelope` / `EntityCallEncoder` / `RemoteEntityCall` high-level pieces are not implemented (design §5/§9 deferred to a later batch)
- The cross-process route refresh protocol (§7 stale route handling) is not implemented
- The schema-driven method dispatch table (§10 step 4) is not implemented

## 11. Relationship with the Ordinary MMO / BigWorld Modes

### Ordinary MMO mode

This layer still has value, because even without distributed cells there are still:

- `Gateway -> BaseApp`
- `BaseApp -> WorldHost`
- `WorldHost -> Gateway`

these internal calls.

### BigWorld mode

This layer becomes a hard requirement, because it additionally adds:

- `Cell -> Ghost`
- `Cell -> Witness`
- `Cell -> AuthorityTransfer`
- `AppMgr -> RouteControl`

## 12. Conclusion

If Apollo's next step is to keep referencing KBE, it cannot look only at process division; it must also fill in the middleware semantics of this "remote entity call" layer.

What really needs closing in:

- Remote entity references
- Method schemas
- Message envelopes
- Route resolution
- Version and authority validation

Only when this layer stands firm will `PlayerAnchor`, `Distributed Space`, and `Ghost/Witness` stop being hard-assembled out of business code.

## Related Reading
- [Base Cell Proxy Object Model](./base-cell-proxy-model.en.md)
- [Witness and Ghost Design](./witness-ghost-design.en.md)
