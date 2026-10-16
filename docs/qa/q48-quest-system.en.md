[English](q48-quest-system.en.md) | [中文](q48-quest-system.md)

# Q48: How to Design a Quest System?

## Core Conclusion

A quest system is not "a list plus a few counters," but a combined system of "quest definition, progress listening, state transitions, and reward granting."

What really needs to be designed clearly first is:

- The quest state machine
- How quest conditions are listened to and updated
- How quest chains and prerequisites are expressed
- How reward granting and quest completion are protected against duplication

If these boundaries are unstable, the more quests there are, the higher the maintenance cost.

## I. What the Quest System Is Really Responsible For

It usually needs to handle:

- Quest acceptance
- Quest condition progression
- Quest completion determination
- Reward granting
- Quest chain unlocking

This means the quest system is both a content-driven system and a state machine system.

## II. First Separate Quest Definition from Quest Instance

### 1. Quest Definition

Describes static rules, for example:

- Quest type
- Prerequisites
- Objective list
- Rewards
- Repeatability rules

### 2. Quest Instance

Describes the player's current state, for example:

- Whether it has been accepted
- Current progress
- Whether it is completed
- Whether the reward has been claimed

If these two layers are not separated, it becomes hard to support daily, weekly, chain, and dynamic quests later.

## III. The Most Critical Part of the Quest System Is State Transition

A common state machine includes:

- Locked
- Acceptable
- In progress
- Submittable
- Completed

Some quests also have:

- Failed
- Abandoned
- Waiting for reset

The key is that every state change must have an explicit trigger condition, rather than being temporarily modified here and there across subsystems.

## IV. Why Quest Progress Updates Cannot Rely Entirely on Polling

If every time you fully scan the player's state to judge whether a quest is complete, the cost is very high.

The more common and more robust approach is event-driven:

- Kill events trigger quest counters
- Pickup events trigger collection quest progression
- Dialogue events trigger story quest progression

In other words, the quest system is more like an event subscriber than a big poller constantly checking itself.

## V. Quest Conditions Should Be Expressed in a Standardized Way

Common quest conditions include:

- Kill specified targets
- Collect items
- Reach an area
- Complete a dialogue
- Use an item
- Reach a certain value

If each type of condition is written as an independent hardcoded flow, maintenance becomes very hard once quest volume grows.

A more reasonable approach is usually:

- Describe with standard condition types plus parameters
- Have the runtime uniformly interpret and advance them

## VI. Quest Chains and Prerequisite Dependencies Must Be Explicit

Complex quest systems are usually not hard because of single quests, but because of the dependencies between quests.

For example:

- Main quests unlock side quests
- Completing A is required to accept B
- Branch rollback or reset after failure

If these dependencies are not explicitly modeled, problems easily appear later:

- Wrong unlocks
- Duplicate claims
- Quests getting stuck

## VII. Reward Granting Must Be Considered Together with Quest State Commitment

Quest completion and reward arrival cannot rely only on the order of client button clicks.

A more robust approach is:

- The server confirms the quest is submittable
- Submission and reward granting go through a unified path
- Reward granting is idempotent

Otherwise disconnect retries or repeated clicks easily produce double grants.

## VIII. A More Robust Engineering Design

Common practice is:

- Quest config tables describe static rules
- Player quest instances record current state
- The event system drives progress advancement
- Quest submission uniformly performs validity checks and reward granting

This makes both content expansion and system stability easier to balance.

## IX. Common Misconceptions

### 1. A quest system is just a configuration problem

Wrong. What is really hard is state transitions and event-driven progression.

### 2. All quests can be freely assembled with a bunch of scripts

Flexible short-term, but long-term it becomes a large number of untraceable exceptions.

### 3. Client showing completion equals quest completion

Wrong. Final completion and reward granting must be confirmed by authoritative logic.

## References

- Various online game quest, achievement, and event-driven progress system practice materials
