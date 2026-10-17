
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q51: How Do You Design an AI System?

## Core Conclusion

The core of a game AI system is not "making monsters smarter", but making behavior more stable, more tunable, and more verifiable under controllable cost.

What really needs to be designed first is:

- How the AI perceives the world
- How the AI makes decisions
- How the AI drives movement, skills, and threat
- How the AI controls cost under large-scale entities

If these boundaries are not clear, the AI will quickly become a pile of hard-to-debug conditional logic.

## 1. What the AI System Is Actually Responsible For

It usually has to handle:

- Target selection
- Patrolling, chasing, disengaging
- Skill usage
- Threat and priority
- Responses to the environment and events

So the AI is more like a composite system of "perception plus decision plus execution".

## 2. First Split the AI into Three Layers

### 1. Perception Layer

Responsible for answering:

- Who do I see
- Who is within threat range
- Am I low on HP
- Is there a danger area nearby

### 2. Decision Layer

Responsible for deciding what to do next based on the current context, for example:

- Keep patrolling
- Switch targets
- Cast a skill
- Retreat or call for support

### 3. Execution Layer

Responsible for turning decisions into actual actions, for example:

- Walk toward the target
- Cast a skill
- Enter idle

If these three layers are not separated, the AI will quickly be written into an unmaintainable pile of flow code.

## 3. How to Choose Among State Machines, Behavior Trees, and Utility Systems

### 1. State Machine

Suitable for:

- Few behavior types
- Simple rules
- Clear state transitions for bosses or small monsters

The advantage is simplicity and intuitiveness; the disadvantage is that complex branches explode as soon as they grow.

### 2. Behavior Tree

Suitable for:

- Many behavior levels
- Need to compose and reuse judgment logic
- Medium-complexity NPCs

It organizes complex behavior better than a pure state machine.

### 3. Utility System or Scoring Decision

Suitable for:

- Need for dynamic comparison among multiple candidate actions
- AI needing to weigh behavior priorities according to the environment

But tuning parameters is more complex.

No single one is always best; the key is the enemy types and the team's debugging ability.

## 4. What AI Design Most Easily Overlooks Is Not "IQ", but Cost

In MMOs or large scenes, the number of AIs is often large.

What really has to be considered is:

- Perception scan frequency
- Decision update frequency
- Pathfinding request frequency
- Whether every monster needs full decision-making

If every AI thinks in full at high frequency, the system will quickly be overwhelmed.

## 5. AI Must Be Decoupled from the Combat System and Pathfinding System

The AI should be responsible for "when to attack, whom to attack, when to disengage", not re-implementing on its own:

- Damage calculation
- Hit logic
- Path search

A more robust structure is usually:

- The AI produces commands
- The combat system and pathfinding system execute authoritative logic

## 6. The Threat System Is Usually a Key Input for AI

Many monster AIs look "smart", but the core is actually just using threat information correctly:

- Who hit me first
- Who healed the target
- Who is close but unreachable
- Who is in the current high-threat state

If the threat system is poorly designed, the AI's target switching will be weird.

## 7. The More Robust Design in Engineering

A common approach is:

- The perception layer produces lightweight context
- The decision layer refreshes by frequency
- The execution layer only calls movement, combat, and skill interfaces
- Normal monsters, elites, and bosses use templates of different complexity

This keeps behavior complexity and performance cost within an acceptable range.

## 8. Common Misconceptions

### 1. The More Complex the AI, the Better

No. In many cases, stable, predictable, and tunable is more important than complex.

### 2. All Monsters Should Use the Same Advanced Behavior Tree

Unrealistic. The complexity requirements of grunts, elites, and bosses are usually different.

### 3. It Is Most Convenient for the AI to Directly Modify State

This tightly couples the AI with the combat, pathfinding, and Buff systems.

## References

- Various MMO / ARPG monster AI, behavior tree, and threat system practice materials
