
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q10: How to implement cross-server features (such as cross-server battlefields, cross-server chat)?

## Core Conclusion

"Cross-server" is not a single unified problem, but at least four completely different categories of problems:

- Cross-server message interworking
- Cross-server relationship interworking
- Cross-server matching and instance scheduling
- Cross-server trading and settlement

Their requirements for real-time-ness, consistency, isolation, and recovery methods are completely different, so they cannot be covered by one single architecture.

A more prudent approach is usually:

- Light-state, weak-consistency features use a central service or event aggregation
- Strong real-time competitive features use a dedicated cross-server gameplay server
- Strong-consistency asset-type features should avoid "directly mutating each other across servers" as much as possible, and instead introduce central settlement or strict compensation mechanisms

---

## 1. First Classify "Cross-server" by Type

Many discussions jump straight to "build a cross-server center" right away, which is usually too coarse.

A more reasonable first step is to classify first.

### 1.1 Message Interworking

For example:

- Cross-server chat
- Server-wide announcements
- Cross-server mail notifications
- Cross-server event broadcasts

Characteristics:

- Moderate real-time requirement
- Slight delay acceptable
- Consistency requirement usually not high
- Best fit for central forwarding or event broadcasting

### 1.2 Relationship Interworking

For example:

- Cross-server friends
- Cross-server blacklist
- Cross-server party
- Cross-server guilds or alliances

Characteristics:

- Need unified identity and relationship index
- Consistency requirement higher than chat
- Both query and write are frequent
- Often need a central relationship service or unified relationship storage

### 1.3 Real-time Competition

For example:

- Cross-server battlefield
- Cross-server arena
- Cross-server instance
- Cross-server event maps

Characteristics:

- Highest real-time requirement
- Frequent runtime state changes
- Usually need to temporarily migrate participating players to a dedicated gameplay server or battlefield server
- Not suitable for being done by continuous remote mutual calls between multiple original servers

### 1.4 Asset Settlement

For example:

- Cross-server trading
- Cross-server auction house
- Cross-server resource transfers
- Cross-server reward issuance

Characteristics:

- Highest consistency requirement
- Highest risk
- Once it goes wrong: duped assets, duplicated deliveries, difficult rollback

This type of feature is usually the least suitable for "every server directly mutating each other's data".

---

## 2. The Most Important Design Decisions in a Cross-server Architecture

### 2.1 First Decide Whether Identity Is Globally Unique

If after going cross-server, player identity is still just an "in-server character ID", many features become complicated.

Usually it is more reasonable to distinguish two layers of identifiers:

- Global account or global player ID
- In-zone character ID

The benefits of doing this are:

- Easier to build cross-server relationships
- Central services can index players more easily
- Easier to do mapping when migrating and returning to the original server

### 2.2 First Decide Who Is the Authority

When doing cross-server, you must first answer:

- Are player assets the responsibility of the original server, or of the central settlement server
- Who is responsible for the real-time combat state in the battlefield
- Are friend relationships cached by the original server, or the responsibility of the central relationship server

If the authority is unclear, later it will become:

- Everyone can mutate
- No one dares to own the responsibility
- When problems occur, impossible to trace responsibility and roll back

### 2.3 First Decide Whether It Is "Data Cross-server" or "Player Cross-server"

These two approaches differ greatly.

#### Data Cross-server

The original servers stay put, only the needed data is synced out.

Suitable for:

- Leaderboards
- Chat
- Announcements
- Relationship queries

#### Player Cross-server

Players are temporarily sent to a cross-server gameplay server, and return to the original server after the gameplay ends.

Suitable for:

- Battlefields
- Arenas
- Cross-server instances
- Real-time event maps

Most real-time cross-server gameplay is essentially more suited to "player cross-server" rather than "multiple original servers remotely coordinating one battle together".

---

## 3. A More Reasonable Overall Architecture

The cross-server system is more like "an independent upper-layer domain" rather than all original servers being pairwise interconnected.

```text
Original Server A ─┐
Original Server B ─┼──► Cross Services
Original Server C ─┘

Cross Services:
- Chat Service
- Social Service
- Match Service
- Battle Service
- Settlement Service
- Global Cache / DB / MQ
```

The reasons for designing it this way:

- Reduce the complexity of mesh connections between original servers
- Unify cross-server protocol and permission boundaries
- Easier to scale out independently
- Easier to unify monitoring and auditing

It is not recommended to directly interconnect every original server with every other original server from the start, for the simple reasons:

- Connection relationships will rapidly balloon
- Routing and permission control become hard to manage
- Troubleshooting is very painful

---

## 4. How to Do Cross-server Chat

### 4.1 A More Reasonable Goal

Cross-server chat is not "let every server directly send messages to each other", but:

- The original server receives the player's speech
- Performs local authentication, rate limiting, muting, and sensitive-word filtering
- Then forwards to the cross-server chat service
- The cross-server chat service is responsible for distributing to the target servers

### 4.2 Recommended Responsibility Split

The original server is responsible for:

- Verifying whether the player can speak
- Local risk control
- Basic message formatting

The cross-server chat service is responsible for:

- Channel routing
- Broadcast distribution
- Writing messages to audit logs
- Cross-server rate limiting and blacklist policies

### 4.3 Why Chat Suits a Central Service

Because it usually has these characteristics:

- Small message bodies
- Can tolerate slight delay
- Does not require strong transactions
- Suits broadcast and subscription models

So chat is one of the features best suited to be made cross-server first.

---

## 5. How to Do Cross-server Battlefields

### 5.1 The Most Common Correct Approach

A cross-server battlefield should usually not have multiple original servers' map logic jointly maintaining one battle.

A more reasonable approach is usually:

1. The original server sends the player into matchmaking
2. The matchmaking center assembles the session
3. Select an independent battlefield server or battlefield instance for that session
4. The player temporarily enters the battlefield server
5. After the battle ends, send the results back to the original server for settlement

### 5.2 Why You Can't Simply "Interconnect Original Servers to Fight One Battle"

Because this immediately runs into many problems:

- Who is the combat authority
- Whose side does skill determination happen on
- Who sends AOI and broadcasts
- Whether one server's lag drags down the whole
- How to recover when a link breaks mid-way

For real-time gameplay, the most stable approach is usually:

To converge one real-time battle into one independent authoritative instance.

### 5.3 Responsibilities of the Battlefield Server

The battlefield server is usually responsible for:

- Combat logic
- In-session state
- In-session leaderboard
- In-session settlement results

The original server is usually responsible for:

- Preparation before the player enters
- Entering and returning
- Rewards landing in accounts
- Long-term asset storage

### 5.4 The Most Critical Boundary

The battlefield server had better not become the final authority for players' long-term assets.

A more prudent approach is:

- The battlefield server produces results
- The original server or the central settlement service does the official posting to accounts

This way, when the battlefield server has an anomaly, it is easier to do retries, compensation, and auditing.

---

## 6. How to Do Cross-server Friends and Cross-server Relationships

### 6.1 The Core Problem of the Relationship System

Cross-server friends is not as simple as "remotely look up character information once".

What really needs to be solved is:

- How to locate players globally
- Who maintains online state
- Where relationship writes land
- How to route cross-server private chat

### 6.2 A More Reasonable Approach

Usually there must be a central relationship service maintaining:

- Friend relationship index
- Blacklist
- Cross-server party relationships
- Online routing map

The original server can cache, but it is not recommended for every server to keep a full copy of the cross-server relationship truth.

### 6.3 How to Handle Online State

The most common approach is:

- The original server reports central online state when the player logs on/off
- The central service maintains a routing table of "the player's current server / the player's current gameplay server"

This way, cross-server private chat, party invites, and friend invites know where to send.

---

## 7. How to Do Cross-server Leaderboards

Leaderboards are one of the cross-server features best suited to central aggregation.

### 7.1 Common Approach

- Each original server periodically reports leaderboard candidate data
- The central leaderboard service does aggregation and sorting
- Use Redis Sorted Set or database to store results

### 7.2 Key Trade-off

Leaderboards usually don't need strong real-time.

So a more reasonable goal is usually:

- Second-level or minute-level convergence
- Guarantee eventual correctness
- Guarantee stable queries

Rather than pursuing that every attribute change is immediately synced across the whole network.

### 7.3 Problems to Watch For

- Leaderboard cycle boundaries
- Deduplication of repeated reports
- Brief display discrepancies caused by data delay
- Season settlement snapshots

The core of leaderboards is not transport, but keeping the statistical caliber and settlement caliber stable.

---

## 8. Why Cross-server Trading Is the Most Dangerous

Cross-server trading, cross-server auction houses, cross-server asset flows are the most sensitive category in cross-server systems.

### 8.1 Risk Points

- Deduction succeeds, delivery fails
- Retries cause duplicated deliveries
- Both sides think they are the successful party
- When rolling back, can't find a unified audit chain

### 8.2 A More Reasonable Approach

Don't let two original servers directly mutate each other's assets.

A more prudent approach is usually:

- Introduce a central settlement service
- All asset changes go through a unified transaction log
- Each transaction has a global transaction ID
- On failure, go through compensation or manual review chain

### 8.3 Principles for This Type of Feature

The keywords of cross-server trading are not "low latency", but:

- Idempotency
- Audit
- Compensation
- Reconciliation

If these are not done well, the more complex the cross-server trading, the greater the risk.

---

## 9. Several Common Shared Problems of Cross-server Features

### 9.1 Routing Problems

You must know:

- Which server the player is currently on
- Whether the player is on a cross-server gameplay server
- Whether the message should be sent to the original server or the gameplay server

### 9.2 Consistency Problems

You must clarify:

- Which features are eventually consistent
- Which flows must be strongly consistent
- Which failures can be retried
- Which failures must be handled manually

### 9.3 Observability Problems

The most feared thing in cross-server problems is "the chain breaks in the middle and nobody knows".

At minimum you must have:

- Global request ID
- Call logs from the original server to the cross-server center
- Routing logs from the central service to the gameplay server
- Key settlement records

### 9.4 Disaster Recovery Problems

If the cross-server center goes down, different features need different degradation strategies:

- Cross-server chat can be briefly unavailable
- Cross-server leaderboards can update delayed
- Matchmaking can pause queue entry
- Ongoing battlefields should be as unaffected as possible

Don't let one central service going down drag down all cross-server features.

---

## 10. A More Pragmatic Landing Order

If you want to build cross-server capabilities step by step, a more reasonable order is usually:

### Phase 1: First Do Lightweight Cross-server

- Cross-server announcements
- Cross-server chat
- Cross-server leaderboards

These features are the easiest to build cross-server infrastructure, and the risk is relatively low.

### Phase 2: Then Do Relationships and Matching

- Cross-server friends
- Cross-server parties
- Cross-server matchmaking

This step starts to require unified identity, online routing, and a central relationship model.

### Phase 3: Last Do Real-time Gameplay and Asset-type

- Cross-server battlefields
- Cross-server instances
- Cross-server trading
- Cross-server auction houses

Because the complexity and risk of these features are clearly higher.

---

## 11. Summary

A cross-server system is not finished by "adding a cross-server center".

What really needs to be distinguished first:

- Is it message interworking, relationship interworking, or real-time gameplay interworking
- Is it data cross-server, or player cross-server
- Who is the authority
- Where is the consistency boundary

For most MMOs, the more prudent experience is usually:

- Chat and leaderboard type features do central aggregation
- Battlefield and instance type features do dedicated gameplay servers
- Trading and asset type features do central settlement and compensation

Don't cover all cross-server needs with one unified approach, otherwise in the end you usually lose performance, clear boundaries, and recoverability all at once.

---

## References

- [KBEngine Lab - Load Balance](https://www.kbelab.com/manual/balance.html)
- [Kafka Documentation](https://kafka.apache.org/documentation/)
- [Redis Pub/Sub](https://redis.io/docs/latest/develop/pubsub/)
