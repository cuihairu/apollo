
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q39: How to design a combat system?

## Core Conclusion

A combat system is not a "damage formula module." It is an authoritative state machine built around combat events.

What really needs to be designed clearly first is:

- Who can initiate a combat request
- What validations a request passes through once it arrives
- How damage, status, aggro, and death are landed
- How results are broadcast and persisted

If these boundaries are not set, the system will be fragile no matter how elegant the numeric formula looks.

## 1. Core responsibilities of a combat system

A combat system usually has to handle:

- Attacks and skill casting
- Hit and damage settlement
- Buff / Debuff triggering
- Aggro and target selection
- Death, revival, and drops

This means it is essentially not a single-point algorithm, but an event chain.

## 2. First establish where authority belongs

In online games, combat results must usually be decided authoritatively by the server.

The client may:

- Submit input
- Play a predicted presentation
- Display combat feedback

But it must not ultimately decide:

- Whether a hit lands
- How much damage is dealt
- Whether a target dies
- Whether anything drops

Otherwise cheats and state divergence are almost inevitable.

## 3. What a single combat request usually goes through

A typical flow generally includes:

1. Input arrives
2. Legality validation
3. Target and spatial condition validation
4. Hit and damage calculation
5. Status application
6. Event broadcast
7. Persistence or ledger recording of key results

Once you lay this chain out straight, many system boundaries become clear.

## 4. The points where a combat system most easily becomes a mess

### 1. Validation and calculation are mixed together

For example, cooldown, resource, target validity, and distance checks are pre-validations. Critical, damage reduction, shield, and lifesteal belong to settlement logic.

If these are mixed in one layer, later maintenance becomes hard.

### 2. Presentation events and authoritative events are mixed together

For example:

- "Slash animation starts" is more presentation-oriented
- "Hit confirmed and HP deducted" is an authoritative event

These two must not be merged into one fuzzy piece of logic.

### 3. Every combat effect is hard-coded into the skill

This leads to very poor reuse and composition. A more reliable design usually splits effects into independent settlement units.

## 5. Which subsystems a combat system usually has to integrate with

At minimum these include:

- Skill system
- Buff system
- Attribute system
- AI system
- Drop system
- Sync and broadcast system

So the combat system is more like a hub than an isolated module.

## 6. A more reliable design line of thinking

A common approach is to split combat into several layers:

### 1. Input layer

Receives combat requests from players or AI.

### 2. Validation layer

Determines:

- Whether cooldown is satisfied
- Whether resources are sufficient
- Whether the target is valid
- Whether the spatial position is legal

### 3. Settlement layer

Handles:

- Hit
- Damage
- Absorption
- Block
- Critical
- Shield

### 4. Application layer

Actually writes results back to object state, for example:

- Deduct HP
- Add or remove buffs
- Switch death state

### 5. Event layer

Broadcasts results to the client and other systems.

## 7. Why a combat system cannot be driven by formulas alone

The damage formula is only a small part.

Much of the complexity comes from:

- Multi-stage skills
- Delayed effects
- Status immunity
- Chain triggers
- Summon ownership
- Death boundary conditions

These are all state machine and event ordering problems, not just formula problems.

## 8. What needs special attention in engineering

### 1. Combat results must be traceable

The key path should ideally allow you to trace out:

- Who initiated
- When the hit landed
- How much damage was dealt
- Why it was reduced

This is very important for investigating numeric anomalies.

### 2. Key results need a clear persistence boundary

Not every combat detail needs to go into the database, but:

- Asset changes
- Death drops
- Instance settlement

Usually need to enter the reliable path.

### 3. Broadcasting must be layered

Many state changes during combat are hot-spot messages and should not all be broadcast in the heaviest way.

## 9. Common misconceptions

### 1. A combat system is just a damage formula

Far from it. Most of the difficulty lies in state progression and event ordering.

### 2. If the client looks like it hit, it should count as a hit

No. The final hit determination must be decided by authoritative logic, possibly combined with lag compensation.

### 3. Putting all combat logic into skill scripts is the most flexible

Flexible in the short term, but it tends to run out of control over the long run, especially trigger chains and debugging cost.

## References

- Various MMO / ARPG combat framework and authoritative settlement practice materials
