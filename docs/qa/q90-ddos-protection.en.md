[English](q90-ddos-protection.en.md) | [中文](q90-ddos-protection.md)

# Q90: How do you defend against DDoS attacks?

## Core Conclusion

DDoS protection is not a problem the application layer can solve alone; it is essentially a capability build of "layered peak shaving and failure domain isolation".

A more realistic approach is usually:

- The upstream network layer blocks large traffic
- The access layer blocks connection and protocol abuse
- The application layer blocks high-cost logic requests

Hoping to withstand a large-scale DDoS by the game server itself is usually unrealistic.

## 1. First distinguish several categories of attacks

Common categories:

- Volumetric attacks: saturating bandwidth
- Protocol attacks: saturating connections and kernel resources
- Application attacks: exhausting business processing capacity

For online games, attacks of "pseudo-normal behavior" also often appear:

- Login floods
- Fake character access
- High-frequency but seemingly legitimate business requests

## 2. Why layered defense is necessary

Because the cost differs greatly between layers:

- The closer to upstream, the cheaper
- The closer to business, the more expensive

So the ideal handling order is:

- Block large traffic at the network and scrubbing layer
- Block abnormal connections and format errors at the gateway layer
- Only relatively trusted requests enter the business server

## 3. The gateway layer is the key defense line

For game systems, the gateway layer usually has to carry:

- Connection rate limiting
- Handshake and protocol validation
- Basic authentication
- Early dropping of invalid requests

The more stable this layer is, the less easily the backend business servers are dragged down.

## 4. The application layer also needs to defend against "low traffic high cost" attacks

Many attacks do not pursue huge traffic, but specifically hit high-cost paths, for example:

- Login verification
- Matchmaking queue entry
- Leaderboard queries
- High-frequency session creation

Such problems need more:

- Interface rate limiting
- Caching and degradation
- High-cost path protection

## 5. Degradation strategies must be prepared in advance

During an attack, not all functions must remain fully normal.

A more robust approach is to predefine in advance:

- Which functions can be turned off or degraded
- Which paths must be kept alive with priority
- Which non-core functions are temporarily suspended

For example:

- Suspend some leaderboard real-time updates
- Restrict registration and certain backend functions
- Tighten high-cost interfaces

## 6. Observation and linkage matter

DDoS protection cannot be judged only by single-machine logs.

Usually at least the following need to be observed in linkage:

- Bandwidth and connection counts
- SYN / ACK anomalies
- Login success rate
- Gateway rejection rate
- Business interface anomaly distribution

Only this way can you judge which layer the attack hit.

## 7. A more robust engineering combination

Common practice is:

- Upstream anti-DDoS or scrubbing services
- Gateway-layer rate limiting and connection protection
- Application-layer rate limiting and degradation
- Risk control and caching added to key business interfaces

This is a system, not the capability of one machine or one middleware.

## 8. Common misconceptions

### 1. DDoS is mainly a bandwidth problem

Incomplete. In many game systems, what really dies first is connection resources and high-cost application-layer paths.

### 2. Having a CDN or anti-DDoS service is enough

Upstream can block a lot of traffic, but application-layer abuse and business hotspots still have to be handled by yourself.

### 3. Degradation plans are only needed when an attack comes

By the time the attack arrives and you design it, it is usually already too late.

## References

- Anti-DDoS, access-layer rate limiting, and application-layer degradation practice materials
