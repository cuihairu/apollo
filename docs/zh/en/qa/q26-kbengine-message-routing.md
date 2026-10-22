
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q26: KBEngine message routing mechanism: how do Gateway, Proxy, and CellApp forward efficiently?

## Core Conclusion

The key to this kind of architecture is not "how many hops the message passes through", but "who is responsible for the connection anchor, who is responsible for session semantics, and who is responsible for scene logic".

In a typical KBEngine-style design, the responsibilities can be roughly understood as:

- `Gateway` is responsible for access and connection carrying
- `Proxy/BaseApp` is responsible for the player session anchor and routing decisions
- `CellApp` is responsible for in-scene authoritative logic

The focus of efficient forwarding is not making all messages take the shortest path, but stably maintaining these three layers of boundaries.

## 1. Why clients cannot directly and randomly connect to CellApp

If the client directly binds to scene logic nodes, several problems immediately arise:

- Connection relationships are hard to switch during scene migration
- Non-scene logic like login, social, and mail has no stable anchor
- Public network connections and high-frequency scene logic are coupled too deeply

So the system usually needs a long-term stable player session entry point.

## 2. What the three layers of responsibility are

### 1. Gateway

The access layer cares more about:

- Connection establishment
- TLS or protocol handshake
- Basic rate limiting
- Connection load balancing

It usually should not carry too much business semantics, otherwise the gateway becomes a hodgepodge.

### 2. Proxy / BaseApp

This is the core anchor of the player session, usually responsible for:

- Account and character binding
- Request authentication
- Routing to the current authoritative logic node
- Carrying session continuity before and after scene migration

Many answers to "who should the client send the message to" essentially come down to: send it to the Proxy first.

### 3. CellApp

CellApp is responsible for:

- Scene and space logic
- Movement, combat, AOI
- Authoritative state advancement of entities

It is not suited to carrying massive public network connection management, but it is very well suited to high-frequency in-scene logic.

## 3. What the typical message flow is

### 1. Client uplink

Common path:

`Client -> Gateway -> Proxy -> CellApp`

Where:

- Gateway guarantees connection reachability
- Proxy judges which logic node the current player should route to
- CellApp executes the real scene logic

### 2. Server downlink

In-scene results usually return to the player anchor, and then the gateway sends them back to the client:

`CellApp -> Proxy -> Gateway -> Client`

The benefit of doing this is that the client only needs to maintain one stable session relationship.

## 4. Why the Proxy is critical

The value of the Proxy is not just "forwarding a message", but solving the problem of separating the player session from scene logic.

It lets the system achieve:

- When the player migrates between scenes, the client connection does not need to be rebuilt frequently
- Non-scene requests still have a stable entry point
- Player-related state has a unified anchor

Without a Proxy, many cross-server, scene-switch, and reconnection flows would be much more complex.

## 5. Key points of efficient routing

### 1. The routing table must be stable

The system needs to quickly know:

- Which Proxy a certain player is currently bound to
- Which CellApp authoritatively manages a certain entity
- Which node is responsible for a certain scene or space

This usually means there must be a stable registration and discovery mechanism, rather than relying on temporary broadcast lookups.

### 2. Do less invalid forwarding

Not all messages need to go around the full path.

For example:

- In-scene broadcasts can be aggregated at the logic layer and then delivered uniformly
- Internal synchronization between services should not go around the public network access link again

Distinguish between the "session anchor path" and the "internal logic sync path".

### 3. Avoid the forwarding layer doing heavy logic at the same time

Once the Gateway or Proxy takes on too much business judgment, the following appears:

- Difficult scale-out
- Expanded failure domains
- Routing coupled with business code

The forwarding layer should know "who to send to", but does not need to carry too much "how to fight, how to calculate" logic.

## 6. Why migration and reconnection both depend on this routing

When the player switches scenes or migrates across Cells, ideally the client should not perceive too many internal node changes.

A more robust approach is usually:

- The Proxy session remains unchanged
- The Proxy updates the player's current logic node mapping
- The authoritative migration is completed between CellApps

Reconnection follows a similar idea:

- First recover to the player session anchor
- Then the anchor connects back to the current logic node

## 7. Common misconceptions

### 1. The Gateway is the central routing hub

Not entirely. The Gateway is more about connection carrying and access; the one that truly understands the player session and logic ownership is usually the Proxy.

### 2. Fewer hops means more efficient

Not necessarily. If one less hop brings strong coupling, complex migration, and fragile reconnection, the whole is not better.

### 3. The Proxy is just a redundant relay

For games with scene migration and complex session semantics, the Proxy is often a key component of system stability.

## References

- Descriptions of BaseApp / CellApp / Proxy responsibilities in KBEngine architecture materials
