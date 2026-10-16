[English](q42-buff-system.en.md) | [中文](q42-buff-system.md)

# Q42: How Do You Design a Buff/Debuff System?

## Core Conclusion

The difficulty of a Buff/Debuff system has never been just "adding a status to a character", but how to keep a large number of status effects controllable under stacking, refreshing, dispelling, immunity, expiry, and trigger chains.

What truly needs to be decided first is a unified set of rules:

- How states are classified
- How same-type states stack
- When they take effect and when they expire
- Who can dispel and who can be immune

Without unified rules, every new status added later creates an exception.

## I. What a Buff System Essentially Is

It can be seen as "a set of sustainable status effects attached to an entity".

Common effects include:

- Attribute additions and reductions
- Damage over time or healing over time
- Control
- Immunity
- Triggers

So it is not a single numeric modification, but a state management system.

## II. Distinguish State Types First

Commonly it can be divided into:

- Attribute modifier type
- Periodic trigger type
- Control type
- Tag type
- Trigger listener type

The value of doing this is letting the system know how to handle each type of state, instead of all Buffs going through the same vague logic.

## III. Stacking Rules Must Be Very Explicit

The place where a Buff system most easily has incidents is non-uniform stacking rules.

At minimum you must define in advance:

- Whether the same ID can stack layers
- Whether same-type effects take the maximum value
- Whether reapplying refreshes the duration or adds a layer
- Whether effects from different sources exist independently

Once these rules are unclear, both the numbers and the logic are hard to keep stable.

## IV. Effective Timing and Expiry Timing Matter Equally

When a Buff starts to take effect is often more critical than how much attack power it adds.

For example:

- Takes effect immediately on cast
- Takes effect on hit
- Takes effect after a delay
- Only starts settling on the next tick

Expiry and removal also need definitions:

- Natural expiry
- Dispelled
- Cleared on death
- Kept or cleared on scene switch

## V. Buffs Should Not Mess Up the Attribute System Directly

A safer approach is usually:

- A Buff produces a set of standardized modifiers
- The attribute system aggregates these modifiers uniformly

Rather than each Buff directly and arbitrarily modifying the character's final attribute fields.

Otherwise you get:

- Hard to track the source
- Hard to restore on removal
- Multiple Buffs overwriting each other

## VI. Control-Type States Need More Caution Than Ordinary Numeric States

For example:

- Stun
- Silence
- Root
- Crowd-control immunity

These states often directly change the skill, movement, and input handling flows, so they cannot be treated as just an ordinary flag.

They usually need to work together with:

- The skill system
- The movement system
- The action state machine

to define priority and conflict rules.

## VII. Trigger Chains Need Special Vigilance

A lot of complexity comes from trigger-type Buffs:

- Reflect damage when hit
- Heal on crit
- Stack layers on kill

If you do not limit the trigger chain, it is easy to get:

- Recursive triggering
- Unstable ordering
- Performance explosion

So you usually need:

- Trigger context
- Trigger depth limits
- Explicit event ordering

## VIII. A Safer Engineering-wise Design

Common practice is:

- Buff configuration defines static rules
- Runtime instances record source, layer count, and remaining time
- Timers or the event system drive the effects
- Attribute modifications are aggregated uniformly
- Dispelling, immunity, and priority have a unified decision table

This makes it easier to control when extending new states later.

## IX. Common Misconceptions

### 1. A Buff Is Just an Attribute Bonus With a Duration

Far from it. Many Buffs are actually continuous state machines.

### 2. Stacking Rules Can Be Added Later

No. Stacking rules are the most fundamental contract of a Buff system.

### 3. Writing Every State as an Independent Script Is Most Flexible

It will be flexible, but it will also quickly become untraceable and unverifiable.

## References

- Various materials on MMO / ARPG Buff systems, state machines, and effect stacking practices
