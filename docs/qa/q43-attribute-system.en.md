[English](q43-attribute-system.en.md) | [中文](q43-attribute-system.md)

# Q43: How to design an attribute system?

## Core Conclusion

An attribute system is not "put a pile of numbers into the character object." It is a numeric organization system that keeps sources clear, defines the calculation order, separates the display result from the combat settlement boundary.

What really needs to be settled first is:

- Which are base attributes
- Which are derived attributes
- Which are temporary corrections
- At what point attributes are finally recalculated

If these layers are not clear, once equipment, buffs, skills, and talents stack on top of each other, the numbers will run out of control.

## 1. What the attribute system actually has to solve

It mainly solves three problems:

- How to represent the character's current numeric state
- How to let multiple sources act together without conflicting
- How to deliver results stably to combat, movement, AI, and other systems

So the attribute system is both a data model and a calculation framework.

## 2. First split attributes by layer

### 1. Base attributes

For example:

- Strength
- Agility
- Constitution
- Base attack
- Base defense

These attributes usually come from character growth and the class template.

### 2. Derived attributes

For example:

- Max health
- Critical hit rate
- Hit
- Dodge
- Attack speed

They are usually derived from base attributes and rule formulas.

### 3. Correction attributes

For example:

- Equipment bonuses
- Buff bonuses
- Set effects
- Talent corrections

This part changes the most frequently.

### 4. Final attributes

These are the results that, after merging and recalculation, are actually used by combat and the display layer.

## 3. Why "source" must always be distinguished

The same final attack power may come from:

- Level growth
- Weapon values
- Buffs
- Temporary skill boosts
- Map or mode bonuses

If the system stores only one final value and records no source, it later becomes very hard to:

- Track down problems
- Revoke effects
- Investigate numeric anomalies

## 4. Calculation order must be an explicit rule

What the attribute system most often produces disputes over is not "does this field exist," but:

- Add the fixed value first, or multiply the percentage first
- Whether bonuses of the same kind stack multiplicatively
- When clamping to upper and lower bounds happens
- How decimals are rounded

All of these must be explicit, unified, and reusable rules.

Otherwise:

- Effects of the same kind will fight each other
- Client and server will easily fall out of sync
- Numeric designers cannot tune stably

## 5. Do not let every subsystem modify final attributes directly

The more reliable approach is usually:

- Each system submits its own attribute correction
- The attribute system aggregates and recalculates them in one place

For example, the buff system should not hard-code "character attack += 10." It should register a correction item with a clear source.

That way, removal, refresh, and stacking are all easier to handle.

## 6. Recalculation strategy matters a lot

Attribute recalculation need not happen every frame, and a full recalculation of every field on each change is also not appropriate.

A common engineering approach is:

- A change sets a dirty flag
- Recalculate locally by attribute group
- Guarantee the result is up to date before anyone reads the final value

This strikes a balance between correctness and performance.

## 7. The boundary between the attribute system and the combat system

The attribute system is responsible for providing stable numeric inputs. The combat system is responsible for using those inputs to settle.

For example:

- The attribute system provides current attack, critical rate, and armor penetration
- The combat system then decides the outcome of a hit and the damage

Do not blur combat formulas and attribute source management completely together.

## 8. A more reliable engineering design

A common approach is:

- An attribute definition table describes fields and calculation rules
- The character object maintains the set of attribute sources
- The attribute system merges and recalculates in one place
- Combat, movement, and AI read only the final results

This keeps sources clear, recalculation controllable, and debugging easier.

## 9. Common misconceptions

### 1. More attributes means more flexibility

Too many fields without layers usually only makes the system more chaotic.

### 2. Caching one final value directly is enough

Without sources and rules, it becomes hard to support complex stacking and investigation later.

### 3. The attribute system only relates to combat

It also affects movement, AI, equipment, buffs, matchmaking scoring, and many other modules.

## References

- Various MMO / ARPG numeric framework and attribute aggregation practice materials
