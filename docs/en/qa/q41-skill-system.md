
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q41: How to Design a Skill System?

## Core Conclusion

The core of a skill system is not "how many fields a skill config has," but turning skills from hardcoded actions into a configurable, validatable, composable execution flow.

What really needs to be designed clearly first is:

- Skill input and target selection
- Cast conditions and cooldowns
- Skill phases and interruption rules
- How skill effects land in the combat system and Buff system

If these boundaries are unclear, the more skills there are, the faster the system spirals out of control.

## I. What a Skill System Essentially Is

A skill system can be understood as "combat behavior templates plus an execution state machine."

A skill usually contains at least:

- Who casts it
- On whom it is cast
- Under what conditions it is cast
- How many phases it executes in
- What effects it produces

It is not a single function call, but a traceable behavior flow.

## II. First Distinguish Skill Definition from Skill Instance

### 1. Skill Definition

The skill definition is static configuration, for example:

- Skill type
- Cooldown
- Cost
- Range
- Target rules
- Effect list

### 2. Skill Instance

The skill instance is runtime state, for example:

- Current casting phase
- Locked targets
- Whether it was interrupted
- Remaining channel time

If these two layers are not separated, it is hard to support channeling, multi-phase, and complex states later.

## III. What Phases Skill Execution Usually Goes Through

Commonly it can be broken into:

1. Initiate the request
2. Pre-validation
3. Enter wind-up or chant
4. Resolve hits and effects
5. Recovery, end, or enter cooldown

For channeling skills, charge-up skills, or multi-phase skills, there are also sustained phases in between.

## IV. Why the Skill System Must Be Decoupled from the Combat System

The skill system is responsible for "how to initiate and organize a skill behavior"; the combat system is responsible for "how effects are actually resolved."

For example:

- The skill system decides this is a fan-shaped (sector) attack
- The combat system decides how much damage each target ultimately takes

If all damage, status, and threat logic is stuffed into the skill module, reuse and debugging will both be poor.

## V. Data-Driven Is the Point, but Not Throwing All Logic into Configuration

The skill system should indeed be data-driven, but the boundary must be clear.

What is usually suitable for configuration:

- Conditions
- Phases
- Effect lists
- Parameters

What is usually not suitable for unlimited configuration:

- Extremely complex condition scripts
- Too many cross-system side effects
- Implicit trigger chains

Otherwise the configuration becomes another kind of hard-to-maintain code.

## VI. Target Selection and Cast Conditions Are Often More Critical Than Effects

In skill design, what most often goes wrong is not the damage numbers, but:

- Who gets locked
- How multiple targets are filtered
- How cast range and facing are determined
- How targetless skills determine placement

These determine whether a skill is predictable, verifiable, and synchronizable.

## VII. Interruption, Immunity, and Conflict Rules Must Be Supported in Advance

The more skills there are, the more a unified set of rules is needed to handle:

- Whether it can be cast while mounted
- Whether it can be cast while moving
- Whether casting can be interrupted by silence or knockback
- Whether similar skills share a cooldown or have independent cooldowns

If these rules are written scattered across individual skills, maintenance cost becomes very high.

## VIII. A More Robust Engineering Organization

Common practice is:

- Skill configuration defines static rules
- Runtime uses skill instances to manage state
- Combat resolution is handled by an independent module
- Effects such as Buffs, summons, and movement execute as standardized effect nodes

This way the skill only organizes the flow and does not carry all business details.

## IX. Common Misconceptions

### 1. A skill system is just a bunch of skill tables

Not enough. Without an execution state machine and a unified effect model, config tables quickly spiral out of control.

### 2. Writing an independent script for each skill is the most flexible

Flexible short-term, but long-term there is lots of duplication, and it is hard to uniformly handle cooldowns, interruptions, and trigger chains.

### 3. If the client can play the animation, the skill succeeded

Wrong. The presentation can play early; final legitimacy and hits must be adjudicated by authoritative logic.

## References

- Various MMO / ARPG skill system and effect orchestration practice materials
