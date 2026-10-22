
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q55: How do you design damage calculation?

## Core Conclusion

Damage calculation is not "writing a formula"; it is embedding a set of numeric rules stably into the combat state machine.

What really needs to be designed clearly first is:

- Which stages damage consists of
- Where attributes come from
- In what order damage reduction, shields, crits, and penetration take effect
- How the final result is clamped and rounded

If the order and boundaries are unclear, the same attributes will compute different results in different scenarios.

## One: What Damage Calculation Really Has to Solve

It has to answer not only "how much health is deducted", but also:

- Did it hit?
- Did it crit?
- Was it blocked or dodged?
- Was it absorbed by a shield?
- Did it trigger damage reflection, lifesteal, chain effects?

So damage calculation is more like a settlement pipeline.

## Two: First Break the Damage Flow Into Stages

A common flow usually includes:

1. Pre-check for legality
2. Hit determination
3. Base damage calculation
4. Skill multiplier and attribute modifiers
5. Crit, penetration, damage reduction, shield, and other modifiers
6. Final clamping and rounding
7. Triggering follow-up effects

The value of doing this is that every effect has a clear insertion point.

## Three: Order Matters More Than the Formula Itself

Many disputes are not that the formula is wrong, but that the order is inconsistent.

For example:

- Is crit applied before or after damage reduction?
- Does the shield absorb before or after final damage?
- Does penetration apply to panel defense or to the final reduction rate?

These must be system-level rules, not each skill computing its own way.

## Four: Damage Calculation Should Be Decoupled from the Attribute System

The attribute system is responsible for providing:

- Attack
- Defense
- Crit rate
- Penetration

The damage system is responsible for using these inputs to settle.

If attribute sources, Buff modifiers, and damage settlement are all crammed together, later troubleshooting becomes very hard.

## Five: Shields, Damage Reduction, and Penetration Are Where Ambiguity Most Often Appears

Because these mechanisms are frequently stacked:

- Fixed reduction
- Percentage reduction
- Shield absorption
- Attribute penetration
- Elemental resistance

Without a unified order, designers and programmers will interpret the same configuration inconsistently.

## Six: The Damage System Must Consider Minimums, Maximums, and Rounding

For example:

- Is zero damage allowed?
- Is the minimum at least 1?
- For decimals, round up, round down, or round half up?

These look like small things, but over time they affect player perception and numeric balance.

## Seven: Random Variance Should Be Kept in Check

Damage variance adds presentation richness, but it should not be too large.

When variance is too large, players feel:

- Results are unpredictable
- Values are unstable
- Balance is hard to understand

So variance is usually just seasoning; it should not become the primary balancing mechanism.

## Eight: A More Solid Design in Engineering

A common approach is:

- Damage settlement runs through fixed stages
- Each stage's inputs and outputs are recordable
- Complex effects are inserted through standardized modifier nodes
- The final result is adjudicated uniformly by the authoritative server

This makes debugging easier and also makes it easier to later extend elemental damage, true damage, DOT, damage reflection, and other mechanisms.

## Nine: Common Misconceptions

### 1. The More Complex the Damage Formula, the More Advanced

No. Complex does not mean good; stable, explainable, and tunable matter more.

### 2. If Panel Attributes Are Right, Damage Cannot Be Wrong

No. Settlement order and special mechanics are often where problems arise.

### 3. Letting Each Skill Write Its Own Damage Formula Is Most Flexible

This quickly makes the system unmaintainable; it is best to build a unified settlement framework.

## References

- Various MMO / ARPG numeric settlement and combat pipeline practice materials
