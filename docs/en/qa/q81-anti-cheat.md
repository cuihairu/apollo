
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q81: How to prevent cheats?

## Core Conclusion

The core of anti-cheat is not "make the client absolutely secure," but to assume the client can always be controlled, and then pull high-value judgments back to the server.

The more pragmatic principles are usually:

- The client is only responsible for input and presentation
- The server is responsible for authoritative adjudication
- High-frequency anomalies rely on behavior detection
- High-value paths rely on idempotency, audit, and risk control

Any design that builds "security" on the assumption that the client is absolutely trustworthy will eventually run into problems.

## 1. First distinguish the essential types of cheats

Common categories can be divided into:

- Client state tampering
- Automation scripts and key simulation
- Network message forgery or replay
- Information wallhacks and illegal visibility acquisition

These types of cheats are countered in different ways, so you cannot rely on a single detection logic.

## 2. Server authority is the first and most important line of defense

Truly critical results should not be decided by the client, for example:

- Whether a position is legal
- Whether a skill hits
- Whether damage is valid
- Whether items and currency changed

The client can submit a request, but it cannot submit the final conclusion.

If this one point is done right, many cheats can only affect the presentation and find it hard to directly rewrite high-value results.

## 3. Client protection has value, but only as a supplement

For example:

- Integrity checks
- Anti-debugging
- Anti-injection
- Resource and memory protection

These measures can raise the cost of cheating, but they cannot serve as the final trust basis.

Because the client is always in an environment controlled by the attacker.

## 4. Behavior detection is usually more practical than single-point signatures

Many cheats do not directly modify packets. Instead they simulate player operations that "look normal."

In that case, behavior-layer analysis is usually more effective, for example:

- Operation frequency beyond human reach
- Abnormally stable monster-farming paths over long periods
- Abnormal reaction times
- Pathing and target selection that are too mechanical

This kind of detection cannot make a single-shot determination, but it is very well suited to risk-control scoring.

## 5. Different modes have different anti-cheat priorities

### 1. Strong real-time competition

More concerned with:

- Aimbot
- Speed hacks
- Wallhacks
- Position and hit anomalies

### 2. MMO / ARPG

More concerned with:

- Auto-farming bots
- Scripted resource farming
- Quest farming
- Abnormal asset flows

In other words, anti-cheat must follow the actual mode. It is not a unified template.

## 6. Risk control and bans must be layered

A more reliable approach is usually not "detect once and permanently ban," but layered handling:

- Mild anomalies: observe first
- Moderate anomalies: restrict gains or raise the verification bar
- High-confidence anomalies: then ban

This reduces false positives and makes it easier to keep iterating on the rules.

## 7. A more reliable engineering combination

A common anti-cheat combination includes:

- Authoritative server settlement
- Idempotency and audit of key operations
- Behavior detection and risk-control scoring
- Client integrity protection
- Backend review and ban policy

The focus is not any single-point technology, but layered defense.

## 8. Common misconceptions

### 1. Packing the client is enough to prevent cheats

No. At best it raises the bar. It cannot establish final trust.

### 2. Just looking at packets can identify all cheats

Many cheats do not modify packets at all. They are automation and information enhancement.

### 3. Anti-cheat is only the security team's job

Many root causes actually lie in business design and server authority boundaries.

## References

- Online game server authority, anti-automation, and risk control practice materials
