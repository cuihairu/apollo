
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q17: What Is Lag Compensation? What Should Be Compensated, and What Should Not?

## Core Conclusion

Lag compensation is not "eliminating network latency", but keeping the damage that latency does to experience and adjudication within an acceptable range.

It usually includes four kinds of means:

- Client prediction for the local player
- Server-authoritative correction
- Interpolation and limited extrapolation for remote entities
- Historical rewind in hit detection

What really needs to be thought through first is not "what advanced algorithm was used", but three things:

- Which results must be finally adjudicated by the server
- Which content is allowed to be shown by the client first and corrected later
- Which gameplay is worth paying complexity for a low-latency experience

## I. First Distinguish State Synchronization from Lag Compensation

State synchronization solves "how the client gets the server state"; lag compensation solves "when a network round-trip exists, how to make operations still look timely and adjudication still fair".

The two often appear together, but they are not the same thing:

- State synchronization focuses on data distribution, frequency, increments, snapshots, AOI
- Lag compensation focuses on prediction, rollback, correction, rewind, interpolation

If these two things are mixed together in discussion, the plan easily loses focus. For example, no matter how frequently snapshots are sent, it cannot automatically solve local operation feel; likewise, no matter how aggressive the prediction, it cannot replace authoritative synchronization.

## II. What Lag Compensation Is Actually Compensating

### 1. Compensating Input Response

After the player presses move, turn, cast, or fire, if you must wait for server confirmation before displaying anything, the experience is very sluggish.

So the common approach is:

- The client immediately executes local prediction
- At the same time sends the input to the server
- The server recomputes under authoritative rules
- The client corrects after receiving the result

This compensates "immediate feedback for the operation", not turning the client into the authority.

### 2. Compensating Display Continuity

Remote entities' position updates always arrive later than the real timeline. If the client directly displays the most recent packet, the picture will jitter, jump, and teleport.

So the common approach is:

- The client caches the last few frames of authoritative snapshots
- When rendering, deliberately lags behind by a small buffer window
- Interpolates between two authoritative points

This compensates "visual continuity", not improving real adjudication precision.

### 3. Compensating Hit Detection Fairness

In gameplay like shooting, collision, and skill hits, the target position seen by the client is often tens to hundreds of milliseconds behind the server's current state.

If the server only adjudicates based on the "current frame target position", players will frequently have the experience of "I clearly hit it but it didn't register".

So many real-time games do historical rewind:

- The client uploads the shot moment, facing, and target information
- The server rewinds to the historical state corresponding to when that player initiated the operation
- Performs one hit detection at that historical moment

This compensates the problem that "the observer's viewpoint and the server's adjudication timeline are inconsistent".

## III. What Suits Compensation and What Does Not

What suits prediction or compensation usually satisfies two conditions:

- The result is reversible or smoothly correctable in the short term
- Even if it is displayed wrong for a moment, the server correction is ultimately acceptable

Typical examples include:

- Your own movement
- Your own facing
- Local cast wind-up or action presentation
- Position and animation display of remote entities

What does not suit letting the client make a "final adjudication" first usually includes:

- Damage settlement
- Asset changes
- Loot ownership
- Quest completion
- Strong-consistency operations like trading, auction, and mail

This content can play feedback first, but the final result must be based on server authority.

## IV. Four Commonly Used Means

### 1. Client Prediction

The most common is movement prediction. The client first simulates a small displacement by input, and the server then returns the confirmation result.

Advantages:

- Direct game feel
- Most effective for continuous input

Costs:

- Must have replay and correction mechanisms
- Prediction logic and server logic must not diverge too much

### 2. Server Correction

After the server-confirmed state comes back, the client has to decide how to correct:

- Small error: smoothly interpolate back
- Large error: pull back directly
- Hard correction for critical errors

The two extremes to avoid here are:

- Never correcting at all, ending up further and further from the authoritative state
- Teleporting correction every time, the picture constantly shaking

### 3. Interpolation and Limited Extrapolation

For other players or monsters, you usually do not predict input, but use interpolation to hide jitter. Only when a snapshot is briefly lost do you do very conservative extrapolation.

The reasons are simple:

- Remote entities have no local input available
- Aggressive extrapolation, once wrong, looks worse than latency

So in engineering the more common approach is "interpolation first, extrapolation as backstop, very short time".

### 4. Historical Rewind

This is the core compensation means for shooting or high-speed collision gameplay. The server needs to keep historical state for a period of time, for example the last 100ms to 300ms of position, facing, and collision-body information.

The process is usually:

1. The client reports the moment the operation occurred and the input parameters
2. The server finds the historical snapshot near that moment
3. Rebuilds the historical state of the relevant entities
4. Uses the historical state for hit or collision detection
5. Then returns to the current timeline and continues running

It solves the time difference between "the target seen" and "the current server state".

## V. Different Game Types Have Different Compensation Priorities

### 1. FPS / TPS

The priorities are usually:

- Low-latency input response
- Historical-rewind hit detection
- High-frequency position synchronization
- Strictly limiting correction jumps

Because this kind of gameplay is sensitive to errors of even tens of milliseconds.

### 2. MMORPG

The priorities are usually:

- Smooth local movement
- Smooth display of remote entities
- Timely feedback for skill presentation
- Final results settled by the server's skill system and combat system

This kind of gameplay often does not need to make every adjudication an FPS-level rewind system, otherwise the cost is too high and the benefit may not match.

### 3. Frame-synchronized Real-time Competitive Games

This kind of gameplay relies more on:

- Input collection
- Lockstep advancement
- Rollback or delayed-input handling

Strictly speaking, this is not entirely "lag compensation" in the traditional MMO state-synchronization system, but another approach centered on deterministic simulation.

## VI. Common Misconceptions

### 1. Thinking More Compensation Is Better

No. Compensation is essentially a trade-off between experience, fairness, and complexity.

The more aggressive the compensation, the more likely it brings:

- Logic divergence
- Correction flicker
- Exploitation by cheats
- Rising server implementation complexity

### 2. Thinking Whatever the Client Sees Should Be Adjudicated

Also no. What the client sees is "an approximate picture after latency, interpolation, and prediction"; it is closer to the experience layer and does not equal the authoritative truth.

### 3. Thinking Only Competitive Games Need Compensation

Wrong. MMOs also need compensation, just with different priorities. MMOs emphasize movement, skill presentation, and smooth entities within AOI; competitive shooting emphasizes hit rewind and extremely low operation lag.

### 4. Thinking Lag Compensation Can Cover Up All Network Problems

It cannot. High jitter, sustained packet loss, severe out-of-order, and local device stutter all make compensation strategies quickly fail. Compensation can only improve experience; it cannot replace network quality itself.

## VII. How It Usually Lands in Engineering

A relatively prudent combination is usually:

- Local player movement does prediction
- The server maintains authoritative state
- Remote entities use interpolation display
- Hit-sensitive gameplay does historical rewind as needed
- Large deviations get hard correction, small deviations get smooth correction
- All assets and combat results are still confirmed by the server

If you go one step further, you also need to design the following together:

- Snapshot frequency and bandwidth budget
- AOI culling
- Historical state cache window
- Correction thresholds
- Degradation strategy under abnormal networks

## References

- Gabriel Gambetta, *Fast-Paced Multiplayer*
- Glenn Fiedler, *Snapshot Interpolation* / *Lag Compensation*
- Valve Developer Community, *Lag Compensation*
