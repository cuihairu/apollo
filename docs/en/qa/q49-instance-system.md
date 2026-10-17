
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q49: How Do You Design an Instance System?

## Core Conclusion

An instance system is essentially not "copying a map", but "creating a controlled scene session for a group of players".

What truly needs to be designed clearly first is:

- How instance instances are created and destroyed
- Who can enter, and when
- How instance state advances
- How failure, exit, disconnect, and reconnect are handled

If you only understand an instance as scene isolation, many problems will surface later in progress saving, reward settlement, and instance reclamation.

## I. What the Instance System Is Really Responsible For

Usually it includes:

- Creating instances
- Binding participating players or teams
- Advancing instance stages
- Managing monsters, mechanisms, bosses, and settlement
- Reclaiming instance resources

So an instance is more like a "temporary scene session system".

## II. Distinguish the Types of Instances First

Commonly you can distinguish along several dimensions:

- Solo, party, team
- Fixed team or matchmaking entry
- Daily, weekly, one-time, event-limited

The core difference between types is not the map, but:

- Entry rules
- Lifecycle
- Settlement method

## III. The Instance Lifecycle Must Be Explicit

A common lifecycle usually includes:

1. Creation
2. Preparation
3. In progress
4. Completed or failed
5. Settlement
6. Reclamation

The key is that every step has clear conditions, rather than just relying on "deleting it when nobody is left in the map".

## IV. Entry Rules and Locking Rules Matter

The instance system usually has to answer:

- Who can enter
- Whether you can join mid-way
- Whether there is a count limit
- Whether team leader confirmation is needed
- Whether it consumes tickets or stamina

Once these rules are not uniform, the client and server easily end up in misaligned states.

## V. Disconnect, Exit, and Reconnect Cannot Be Added Later

This is one of the most common real problems in instance systems.

You must define in advance:

- Whether the position is kept after a disconnect
- Whether reconnect allows returning to the original instance
- Whether an active exit can re-enter later
- How long the instance is retained after the whole team leaves

These directly determine the experience and the resource reclamation strategy.

## VI. Progress Saving Should Be Layered by Instance Type

Not all instances should save complete progress.

For example:

- A single-run combat instance may only need in-session state
- A long-cycle team instance may need to save boss kill progress
- A roguelike or tower-climbing instance may need to save stage checkpoints

So "progress saving" is not a uniform template, but a gameplay decision.

## VII. Reward Settlement Must Be Separated from Instance State

Instance completion does not equal rewards having been safely granted.

A safer approach is usually:

- The instance state first enters the completed state
- The settlement system then grants rewards by rules
- The reward-granting chain is idempotent

Otherwise double grants or missed grants are very likely during disconnects, retries, and repeated confirmations.

## VIII. A Safer Engineering-wise Design

Common practice is:

- The instance template defines static configuration
- The instance object maintains runtime state
- Players and teams are explicitly bound to the instance
- Settlement and reward granting are a separate chain
- Empty instances are reclaimed on a timer

This makes lifecycle and resource management clearer.

## IX. Common Misconceptions

### 1. An Instance Is Just a Copy of a Scene

Not enough. What is more critical about an instance is the session, progress, and settlement.

### 2. Just Grant Rewards Directly When the Instance Ends

No. Reward granting must consider idempotency, disconnect, and retry.

### 3. Destroying the Instance the Moment Nobody Is in It Saves the Most Resources

This directly destroys the experience of reconnect and brief disconnect recovery.

## References

- Various materials on MMO / ARPG instance sessions, progress saving, and settlement practices
