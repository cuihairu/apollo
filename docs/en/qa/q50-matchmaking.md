
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q50: How do you design a matchmaking system?

## Core Conclusion

A matchmaking system really has to balance three things:

- Fairness
- Waiting duration
- Explainability

There is no matchmaking algorithm that is always optimal; there are only trade-offs more suitable for a specific gameplay and online scale.

If you only stare at "closest strength", the waiting time gets longer; if you only stare at "instant start", the match quality is very poor. A matchmaking system is essentially a dynamic balancer.

## 1. First clarify what the matching object is

Different gameplay matches very different objects:

- Single player matching single player
- Team matching team
- Multi-character class composition matching
- PvP rank matching
- PvE team-up matching

If the object model is not defined clearly, the later scoring and team-up logic will be chaotic.

## 2. Which dimensions a matchmaking system really cares about

Common ones include:

- Strength or rank
- Waiting time
- Team size
- Class composition
- Server or region
- Network quality

Not every gameplay needs the full set of dimensions, but the primary and secondary must be determined first.

## 3. Fairness and waiting duration are necessarily in conflict

This is the basic reality of a matchmaking system.

Stricter matching conditions mean:

- Closer matches
- Longer waits

Looser matching conditions mean:

- Faster starts
- More unstable match quality

So many systems adopt the strategy of "gradually widening the match window".

## 4. Matchmaking pool design matters more than formulas

Many people focus all their attention on the scoring algorithm, but what is actually more important is often matchmaking pool management.

What needs to be clear:

- How players enter the pool
- How teams are represented
- How timeouts are handled
- How to return to the pool after accept-confirmation failure

If this part is not designed well, even the best scoring function cannot land.

## 5. Different gameplay suits different matchmaking focuses

### 1. Competitive PvP

Emphasizes more:

- Close strength
- Fair teams
- Reasonable rank and hidden score

### 2. PvE team-up

Emphasizes more:

- Start speed
- Complete role responsibilities
- Basic combat power threshold

### 3. Large battlefields or events

Emphasizes more:

- Quickly gathering headcount
- Faction balance
- Cross-server aggregation capability

## 6. After a successful match, the "accept phase" still needs handling

Many systems do not enter directly after matching, but instead have:

- Accept confirmation
- Timeout cancellation
- Fill-in for missing players

This phase is important because it can significantly reduce the waste of "instantly abandoning after being matched".

## 7. Cross-server matchmaking introduces extra constraints

Once cross-server, the system usually also has to consider:

- Region/server latency
- Language or region
- Event time windows
- Instance server carrying capacity

So cross-server matchmaking is not only enlarging the pool, but also enlarging scheduling complexity.

## 8. A more robust engineering design

Common practice is:

- First build a clear matchmaking object model
- Use segmented scoring and gradual window widening to control fairness and waiting
- Enter the accept confirmation phase after a successful match
- Finally hand off to the instance or battlefield instance system to carry

This way the boundary between matchmaking and instance creation is also clearer.

## 9. Common misconceptions

### 1. The more complex the matchmaking algorithm, the better

No. What matters more in many cases is that rules are explainable, tunable, and observable.

### 2. As long as the hidden score is accurate, matchmaking will have no problems

No. Team composition, class structure, regional latency, and pool size all affect the experience.

### 3. A matchmaking system is just a sorting algorithm

Far from it. It is also a queue system, a scheduling system, and an experience balancing system.

## References

- Various PvP / PvE matchmaking systems, hidden score, and window-widening strategy practice materials
