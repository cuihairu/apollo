
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q12: How do you handle network jitter and packet loss?

## Core Conclusion

Handling network jitter and packet loss, the goal is not to "make the network better", but to keep the system playable, controllable, and recoverable under bad networks.

First, distinguish three things:

- Which problems can be masked at the client visual layer
- Which problems must be recovered at the transport layer
- Which problems must be maintained by the server with authoritative state

A truly mature approach is usually not a single technique, but multi-layer coordination:

- The client does interpolation, extrapolation, buffering, and smoothing
- The transport layer does sequence numbers, retransmission, and packet loss statistics
- The server does authoritative correction, rate limiting, and state convergence

---

## 1. First distinguish the several kinds of problems in the network

When discussing "bad network", it is easiest to lump different problems into one category.

### 1.1 Latency

Messages arrive slowly, but relatively stable.

Manifestations:

- Slow operation response
- Delayed skill feedback
- An overall dragging feeling in character actions

### 1.2 Jitter

Latency is not constant, but fluctuates up and down.

Manifestations:

- The character is smooth one moment and stutters the next
- Position update intervals are uneven
- The screen shows slight stutters and jumps

### 1.3 Packet loss

Some packets simply never arrive.

Manifestations:

- Operations occasionally fail
- Position suddenly jumps
- Some state updates are missing

### 1.4 Out of order

Packets arrive, but not in the order they were sent.

Manifestations:

- Old state overwrites new state
- A feeling of moving backwards
- Actions occur in the wrong order

These four kinds of problems appear together, but the handling is not the same.

---

## 2. First decide which messages are worth "saving"

How well network problems are handled — the core is not the algorithm, but whether messages have been layered first.

### 2.1 Messages that must be recovered

For example:

- Login result
- Skill cast confirmation
- Damage settlement
- Transaction result
- Inventory modification

Such messages cannot simply be dropped, otherwise the logic will be directly wrong.

### 2.2 Messages where old can be dropped in favor of new

For example:

- Position sync
- Facing sync
- High-frequency action state
- High-frequency presentation-layer broadcasts

For such messages, even if old packets arrive completely, they are meaningless; what matters is that the latest state arrives quickly.

### 2.3 Messages that can be smoothed purely at the presentation layer

For example:

- Remote player movement display
- Remote player turn presentation
- Certain low-value animation triggers

The focus here is not retransmission, but not letting the player see jumps.

---

## 3. Core handling of jitter: do not play directly by arrival time

The most typical problem with jitter is:

Packets do not arrive at stable intervals.

If the client renders immediately upon receipt, the result is often:

- One frame with no data
- The next frame gets two or three packets
- The screen speed varies

### 3.1 A more reasonable approach: delayed playback

The client should not chase the "latest arrived packet" and play it immediately, but deliberately lag behind by a small window.

For example:

- The server sends position every 100ms
- The client deliberately lags 100ms to 200ms when displaying

The benefits are:

- Can wait for late-arriving packets
- Can smoothly interpolate between two samples
- Visually more stable

The cost is:

- The screen gains a fixed additional delay

This is the classic "trading stability for a bit of timeliness".

### 3.2 Why a jitter buffer is not better the larger it is

The larger the buffer, the steadier the picture, but the duller the feel.

So the key to a jitter buffer is not "as large as possible", but:

- Large enough to cover typical jitter
- Not dragging interaction latency too high

In many cases it should be a dynamic value, not a hardcoded constant.

---

## 4. The three most common masking techniques on the client

### 4.1 Interpolation

Interpolation suits:

- Already having two samples, before and after
- Just wanting smoother presentation

Typical scenarios:

- Remote character movement
- Remote character turning
- Remote position broadcast display

Its advantage is stability; its disadvantage is that it necessarily introduces display delay.

### 4.2 Extrapolation

Extrapolation suits:

- New samples temporarily not arrived
- But short-term prediction is possible from old velocity and old direction

Typical scenarios:

- High-frequency moving objects
- Packets lost for a short time

Its risk is:

- The worse the network, the larger the error

So extrapolation generally cannot be done indefinitely; it usually only suits a very short time window.

### 4.3 Correction

After client prediction or extrapolation, alignment with the server's authoritative state is still needed eventually.

Correction usually comes in two kinds:

- Soft correction: small errors smoothly pulled back
- Hard correction: large errors directly corrected

Without this layer, the client will only drift further and further off.

---

## 5. Core handling of packet loss: distinguish "retransmit" or "ignore"

Not all packet loss should be retransmitted.

### 5.1 Cases where retransmission is required

For example:

- Skill cast events
- Important state transitions
- Settlement results
- Reliable confirmation messages

Because losing them is not "a stutter", but a logic error.

### 5.2 Cases where retransmission should not happen

For example:

- Position packets from 300ms ago
- Already outdated posture packets
- Old state in high-frequency broadcasts

Because even if they are recovered, they only create more out-of-order and visual noise.

### 5.3 So packet loss handling must be bound together with message layering

The right question is not "does the system support retransmission", but:

- Which messages need retransmission
- Which messages should be dropped directly
- Which messages only need the latest copy

---

## 6. What the server must do

The client can mask the experience, but cannot replace the server's authoritative control.

### 6.1 Maintain authoritative state

No matter whether the client interpolates, extrapolates, or predicts, the server should be:

- The authoritative source of position
- The authoritative source of combat results
- The authoritative source of asset state

Otherwise, under weak networks the client will quickly drift the state off track.

### 6.2 Perform state convergence

The server needs to allow the client to temporarily deviate, but eventually pull the system back onto the authoritative track.

Typical means include:

- Periodic state snapshots
- Sequence number checks
- Timeout discarding of too-old input
- Periodic correction of position and state

### 6.3 Control send frequency

Sometimes network jitter and packet loss are not because the external network is too bad, but because the link is flooded by ourselves.

So the server also needs to do:

- Rate limiting
- Packet merging
- Downsampling
- AOI culling
- Priority sending

Don't blame everything on "bad network" to the carrier.

---

## 7. Should send frequency be reduced when the network is bad?

Usually yes.

Because continuing high-frequency sending in a bad network environment commonly results in:

- Queue buildup
- More packet loss
- More severe out-of-order
- Longer recovery time

A more reasonable adaptive strategy is usually:

- High-frequency sync when the network is good
- Moderate frequency reduction when the network is average
- Only keep high-priority messages when the network is bad

In other words:

When the network degrades, it is not "send desperately", but "send more selectively".

---

## 8. A more practical handling framework

### 8.1 Presentation layer

Handling goals:

- Don't let the player see obvious stutter and teleporting

Common methods:

- Interpolation
- Extrapolation
- Jitter buffer
- Smooth correction

### 8.2 Transport layer

Handling goals:

- Distinguish reliable messages from unreliable messages
- Be able to measure packet loss and RTT
- Be able to retransmit key messages

Common methods:

- Sequence numbers
- ACK / NACK
- Retransmission queues
- Packet loss rate statistics

### 8.3 Logic layer

Handling goals:

- Maintain server authority
- Control state drift
- Avoid old state polluting new state in reverse

Common methods:

- Timestamp validation
- State version numbers
- Expired packet dropping
- Periodic authoritative write-back

---

## 9. Several common misconceptions

### 9.1 Misconception one: having retransmission is enough

No.

Retransmission solves the reliability of key messages; it does not solve visual jitter, nor the problem that old state is already worthless.

### 9.2 Misconception two: if the client smooths, no server correction is needed

No.

Client smoothing is only presentation decoration, not logical truth.

### 9.3 Misconception three: under weak networks, make all messages reliable

This usually makes things worse.

Because you will also recover a large number of low-value old messages, further increasing congestion and head-of-line blocking.

---

## 10. Summary

The key to handling jitter and packet loss is not betting on a single technique, but first layering the messages, then handling them layer by layer.

More practical principles are usually:

- Key events must be reliably recovered
- High-frequency state should allow dropping old in favor of new
- The client is responsible for making the picture stable
- The server is responsible for pulling the logic back to authority

If the boundaries of these layers are clear, then even if the experience degrades under weak networks, the system can still remain playable and convergent.

---

## References

- [KBEngine GitHub - Network](https://github.com/kbengine/kbengine/tree/master/kbe/src/server/network)
- [Jitter Buffer](https://en.wikipedia.org/wiki/Jitter_buffer)
- [Forward Error Correction](https://en.wikipedia.org/wiki/Forward_error_correction)
