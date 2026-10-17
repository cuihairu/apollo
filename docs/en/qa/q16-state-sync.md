
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q16: What Is the Synchronization Problem? How Do Client and Server State Stay in Sync?

## Core Conclusion

The "synchronization problem" is essentially not "how to copy all state identically to the client", but:

- The world seen by the server and the client is naturally out of phase
- How to keep the logic authoritative
- How to make the presentation smooth enough
- How to keep the error within an acceptable range

So what state synchronization really has to solve is a three-layer problem:

- Who holds the authoritative state
- Which state must be synchronized
- How the client hides latency while waiting for synchronization

For MMOs, the main approach is usually:

- Server authority
- State synchronization as the primary mechanism
- Client prediction, interpolation, and correction as presentation compensation

Rather than full pure frame synchronization.

---

## I. Why the Synchronization Problem Must Exist

In online games, the client and server cannot naturally stay completely in sync.

The reasons are simple:

- The network has latency
- Messages have jitter and packet loss
- Client rendering and server logic are not on the same cadence
- Each client only sees the part of the information it received

So in any multiplayer game, the following naturally appear:

- Server state
- Local client state
- Other clients' state

They are not exactly the same at the same moment; this is normal.

The real questions are:

- Which inconsistencies are acceptable
- Which inconsistencies must be corrected

---

## II. First Distinguish "What Is Being Synchronized"

Many discussions jump straight to "do state synchronization" without saying which category of thing is actually being synchronized.

A more reasonable division has at least four categories.

### 2.1 Position and Movement State

For example:

- Coordinates
- Velocity
- Facing direction
- Current movement state

This kind of state changes frequently and is heavily affected by the network.

### 2.2 Core Logic State

For example:

- HP / MP
- Buff / Debuff
- Skill cooldowns
- Alive/dead state

This kind of state directly affects logic correctness.

### 2.3 Events

For example:

- Skill cast
- Taking a hit
- Death
- Entering combat

Events and state are not the same thing.

An event is "something happened once"; state is "what things are like now".

### 2.4 Presentation-layer Information

For example:

- Animation playback
- VFX triggers
- Floating combat text
- Sound effects

This kind of information is usually more presentation-oriented and can tolerate some error.

If these categories are not separated first, later you will end up with:

- Transmitting events as if they were state
- Re-sending state as if it were an event
- In the end, both the protocol and the correction logic become a mess

---

## III. First Decide Who Is the Authority

The first principle of the synchronization problem is: there must be an authority.

### 3.1 Why the Server Is Usually the Authority

Because it has to be responsible for:

- Anti-cheat
- Unified adjudication
- Guaranteeing eventual consistency
- Handling multi-player interaction conflicts

If the authority is not on the server, it is very easy to end up with:

- Each client telling its own story
- Asset state being tampered with
- Combat results that cannot be unified

### 3.2 Can the Client "Move First"?

Yes, but that does not make the client the authority.

More precisely:

- The client first does prediction or presentation
- The server confirms in the end
- The client then aligns to the server state

In other words, the client can act first, but cannot make the final call.

---

## IV. Synchronization Is Not Just the Two Words "State Sync vs Frame Sync"

### 4.1 State Synchronization

The core idea of state synchronization is:

- The server maintains authoritative state
- The client periodically receives state updates
- The client does interpolation, extrapolation, and correction

It suits:

- MMORPG
- Open-world games
- Scenes with a large number of entities at the same time

Because in these games, the world the player sees is inherently not a full global view.

### 4.2 Frame Synchronization

The core idea of frame synchronization is:

- All sides synchronize inputs
- Execute identical logic under the same frame step
- Rely on determinism to keep state consistent

It suits:

- RTS
- Fighting games
- Gameplay with few units and high determinism

It does not suit most MMO main-scene scenarios, for reasons including:

- Different fields of view
- Huge entity counts
- Obvious local-area hotspots
- In open worlds, it is hard to guarantee that globally unified frame logic stays affordable

### 4.3 The Real-world Approach That Is Actually Common

Most MMOs are actually hybrid:

- Main world core logic: state synchronization
- Certain mini-games, session-based gameplay: may approach frame synchronization or semi-lockstep
- Client presentation layer: prediction + interpolation + correction

So don't understand the synchronization problem as "you can only pick one of two".

---

## V. The Real Core Chain of State Synchronization

A more common server-authoritative chain is usually:

1. The client sends input or an operation request
2. The server validates and updates the authoritative state
3. The server broadcasts the state change to the relevant clients
4. The client uses presentation-layer means to smoothly transition to the new state

The most important things in this chain are:

- Whether the client sends "intent" or "result"
- Whether the server broadcasts "events" or "latest state"
- How the client pulls itself back when it sees a deviation

---

## VI. Why the Client Must Do Prediction and Interpolation

If the client waits for server confirmation every time before moving, the experience is usually very poor.

### 6.1 Common Approach for the Local Player: Prediction

For example, during local movement:

- The client immediately moves the character
- At the same time sends the input to the server
- The server confirms it is legal and continues synchronizing
- If inconsistent, then correct

The purpose of doing this is:

- To guarantee game feel

The cost is:

- You must accept "occasional pull-backs"

### 6.2 Common Approach for Remote Players: Interpolation

For remote players, the client generally does not do aggressive prediction; instead it more commonly:

- Receives a series of position samples
- Smoothly interpolates between samples

The purpose of doing this is:

- To keep the picture stable
- To avoid jumps

### 6.3 Common Approach on Packet Loss: Short-term Extrapolation

If a new packet has not arrived for the moment, you can predict for a short time based on the old velocity.

But extrapolation cannot be done indefinitely, because the longer the time, the larger the error.

---

## VII. Correction Is the Most Underestimated Part of a Synchronization System

If you only do prediction and never correct, the system will definitely drift.

### 7.1 Small Error: Soft Correction

If the deviation is not large, you should usually:

- Smoothly pull back
- Avoid the player seeing a teleport with the naked eye

### 7.2 Large Error: Hard Correction

If the deviation is already obvious enough to affect logic, you can no longer pull slowly.

At this point you usually have to:

- Directly correct to the authoritative state

### 7.3 Correction Must Not Only Look at Position

Many systems only focus on position correction, but in reality you also have to consider:

- Facing direction
- Velocity
- Skill state
- Buff state
- Whether it hit, whether it died

Otherwise the surface position is aligned but the logic is still wrong.

---

## VIII. In MMOs You Also Have to Add AOI and Visibility Culling

Synchronization is not broadcasting the whole world to all clients.

The most critical layer in an MMO is:

- Only synchronize entities within the player's current area of interest

This is why synchronization systems and AOI are usually tightly bound together.

### 8.1 Why AOI Culling Is Mandatory

Because if you do not cull:

- Bandwidth will explode
- The client cannot keep up
- Synchronization latency will get higher and higher

### 8.2 So the MMO Synchronization System Is Naturally Local Synchronization

This is also a fundamental difference from pure frame-synchronized gameplay:

- Not everyone must get the same global state
- Instead, each person gets "the part of the state relevant to themselves"

---

## IX. The Most Common Optimization Directions for State Synchronization

### 9.1 Incremental Synchronization

Not sending the full state every time, but only the changed parts.

Suits:

- Medium/low-frequency state changes
- Systems with a stable baseline state

### 9.2 Priority Synchronization

Different states have different priorities.

For example:

- Player position, damage results: high priority
- Environment objects, low-value presentation: low priority

When the network is tight, low-priority messages can be downgraded in frequency or even skipped for a while.

### 9.3 Frequency Tiering

Different objects should not have the same synchronization frequency either.

For example:

- Combat entities near the local player: high frequency
- Ordinary monsters far away: medium frequency
- Environment objects: low frequency

### 9.4 Compression and Culling

Truly effective optimization is usually a combination:

- Field culling
- Incremental synchronization
- AOI restriction
- Message compression

Rather than relying on only one of them.

---

## X. Several of the Most Common Misconceptions

### 10.1 Misconception One: Synchronization Means Sending Out All State

No.

The focus of synchronization has never been "send everything", but:

- Send what is necessary
- Send it to the right people
- Send it at the right frequency

### 10.2 Misconception Two: The More Client Prediction the Better

Also no.

The more prediction, the greater the correction cost and the wrong presentation.

Prediction should only be used in scenarios where:

- The experience benefit is clear
- It can be pulled back by the authoritative state

### 10.3 Misconception Three: State Synchronization and Event Synchronization Can Be Designed Mixed Together

If you do not distinguish events from state, in the end you often get:

- Events executed repeatedly
- New events overwritten by old state
- Rollback and re-send logic in chaos

These two concepts must be separated.

---

## XI. A More Practical Understanding Framework

If you want to explain the synchronization problem clearly, a better framework is usually:

1. The server is responsible for authoritative logic
2. The client is responsible for experience compensation
3. State synchronization is responsible for roughly aligning the world
4. Event synchronization is responsible for expressing "what happened"
5. Correction is responsible for pulling deviations back
6. AOI is responsible for controlling the synchronization scope

Separating these layers makes the whole system much clearer.

---

## XII. Summary

The essence of the synchronization problem is not pursuing that the client and server are absolutely consistent at every instant, but:

- Logically, the server is the reference
- Presentation-wise, as smooth and natural as possible
- Under weak networks and latency, still able to converge to the same result

For MMOs, the more common and more prudent approach is usually:

- Server authority
- State synchronization as the primary mechanism
- Event synchronization as a supplement
- Client prediction, interpolation, extrapolation, and correction as experience compensation
- Combined with AOI and priority to control synchronization cost

This is what makes a usable synchronization system, rather than just shouting "use state synchronization" on its own.

---

## References

- [KBEngine GitHub - Entity Alias](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/entitydef/entity_alias.h)
- [Gaffer On Games - Networked Physics](https://gafferongames.com/post/networked_physics_2004/)
