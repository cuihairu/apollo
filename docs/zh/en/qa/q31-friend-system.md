
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q31: How Do You Design a Friend System?

## Core Conclusion

A friend system looks like a social feature, but it is actually a composite system of "relationship storage plus online presence plus interaction permissions".

What really needs to be thought through first is not interface details, but:

- Whether relationships are bidirectional or unidirectional
- How online state propagates
- How blacklist and permissions are adjudicated
- How relationships are maintained after cross-server and partitioning

If these boundaries are not defined clearly, later features pile up messily.

## 1. First Define Relationship Types

A friend system usually has more than one kind of relationship.

Common ones include:

- Bidirectional friends
- Unidirectional follows
- Blacklists
- Special relationships, such as intimacy, master/apprentice, CP

The storage models and notification rules of these relationships are all different; they should not be mixed into one fuzzy logic.

## 2. The Real Core Capabilities of a Friend System

It usually includes at least:

- Establishing and dissolving relationships
- Friend list queries
- Online state viewing
- Interaction entries such as private chat, invites, and teleports
- Blacklist interception

On the surface it is "adding friends"; in essence it is social relationships and permission adjudication.

## 3. How to Model Relationship Data

The most common approach is a relationship table, rather than stuffing the whole friend list into the character master record.

The reasons are simple:

- Relationships are naturally many-to-many
- They need separate queries and pagination
- They need to carry extra information such as remarks, groups, and intimacy

Bidirectional friends usually have two modeling approaches:

- One record representing a pair of relationships
- Two directional records representing each side's perspective

Which to choose depends on query and maintenance habits.

## 4. Why Online State Is the Hard Part

The static relationships of a friend system are not hard; what changes frequently is online state.

The difficulty lies in:

- Players frequently going online and offline
- Possibly crossing gateways and servers
- State pushes easily forming fan-out

So many systems do layering:

- Relationship data goes to the persistence layer
- Online state goes to a cache or session layer
- Notifications are by subscription or incremental update

## 5. Do Not Make the Friend System a "Full Push System"

When the friend count is large, pushing the full state to all friends on every state change is very costly.

A more reasonable approach is usually:

- Pull a summary on login
- Push only incremental changes on online/offline
- Relationships with no interaction for a long time get lower update frequency or lazy queries

## 6. Blacklist and Permissions Must Have Higher Priority

Many systems treat the blacklist as an add-on feature, which is dangerous.

The more reasonable principles are:

- Blacklist takes priority over friend relationships
- After being blacklisted, entries such as private chat, invites, and applications should be blocked
- Certain display information should also be restricted

Otherwise the logic will conflict with itself.

## 7. What Needs Extra Consideration in Cross-Server Scenarios

If the game has cross-server, partitioning, or multi-scene clusters, the friend system usually needs to solve:

- Globally unique character identifiers
- Unified online state sources
- Cross-server private chat and invite routing
- Offline messages or reminders

At this point the friend system is no longer just a single-server table structure problem, but a social platform problem.

## 8. The More Robust Combination in Engineering

A common approach is:

- The persistence layer saves friend relationships and static data such as remarks
- Redis or an online service saves an online state summary
- A social service uniformly handles applications, blacklists, invites, and private chat routing

This way the core game logic is not dragged down too heavily by social state.

## 9. Common Misconceptions

### 1. The Friend System Is Just a Relationship Table

Not enough. Online state, permission control, and cross-server routing are often more complex.

### 2. Querying the Character Process Directly for Online State Is Fine

In a distributed environment this is very heavy, and not conducive to unified aggregation.

### 3. The Blacklist Is Only Chat Filtering

Usually it is not. It also affects invites, applications, profile viewing, and other entries.

## References

- Various online game social systems and online state service practice materials
