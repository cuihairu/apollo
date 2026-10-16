[English](q20-heartbeat-reconnect.en.md) | [中文](q20-heartbeat-reconnect.md)

# Q20: How does a long connection maintain heartbeat? How is reconnection designed?

## Core Conclusion

Heartbeat and reconnection are two related but different things:

- Heartbeat solves "is the connection still alive"
- Reconnection solves "how does the business continue after the connection drops"

Many systems mistake "can automatically reconnect" for "user state won't be lost". This is wrong. Reconnection only re-establishes the link; whether the state is recoverable also depends on the session, cache, message resend, and authoritative state preservation strategies.

## 1. Why the application layer still needs heartbeat

A TCP connection has a typical problem: the physical link is already broken, but both ends may not be able to perceive it immediately.

Common scenarios include:

- Phone switching networks
- Wi-Fi dropped without the system reporting it in time
- NAT mapping timeout
- Intermediate devices silently dropping idle connections

In these cases the application layer needs to actively probe, otherwise both the client and the server may mistakenly believe the connection still exists.

Application-layer heartbeat generally has three purposes:

- Timely discovery of fake connections
- Keeping NAT or intermediate links alive
- Incidentally measuring RTT, jitter, and packet loss

## 2. Heartbeat is not the same as TCP Keepalive

TCP Keepalive is an operating-system-level mechanism, but the default parameters are usually too conservative and cannot meet real-time game requirements in many environments.

So the common practice in the field is:

- TCP Keepalive as a bottom-line safeguard
- Application-layer heartbeat as the primary detection means

The advantages of the application layer are:

- Controllable interval
- Can carry business context
- Can measure latency
- Can unify across TCP, WebSocket, UDP, QUIC, and other links

## 3. How heartbeat should be designed

### 1. Keep the heartbeat packet as simple as possible

A practical heartbeat packet usually only needs:

- Session identifier
- Timestamp
- Sequence number

Add when necessary:

- Current client time
- Recent network quality information
- Lightweight ACK information

Don't turn the heartbeat packet into a business hodgepodge, otherwise the frequency cost rises, and traffic pressure on abnormal links is amplified.

### 2. When there is business traffic, a separate heartbeat is not always needed

If business messages are frequent on the connection, many systems treat "any inbound or outbound traffic" as an activity signal.

This is more reasonable than mechanically sending a fixed packet every few seconds, because:

- It reduces meaningless packets
- It better matches the true activity state
- It saves bandwidth for high-frequency real-time gameplay

### 3. The timeout threshold must not only consider ideal networks

The heartbeat interval and timeout threshold need to account for:

- User network jitter
- GC or main-thread stalls
- Mobile app background switching
- Short-term congestion on intermediate devices

Common engineering values are:

- Heartbeat interval of 3 to 10 seconds
- Timeout threshold set to 2 to 3 heartbeat periods

But this is not a fixed formula; it still depends on the gameplay and link environment.

## 4. How the server determines a disconnected link

The server usually does not only look at "whether a pong was received", but at "whether any valid message for this connection was received recently".

The typical judgment method is:

1. Each session maintains a last-received time
2. Periodically scan for timed-out connections
3. After timeout, first mark as lost, then enter the reclamation process
4. Keep necessary context within the window that allows reconnection

Special attention here: timeout strategies for different types of connections are not necessarily the same:

- Client connections care about experience and recovery
- Internal service connections care about stability and degradation
- Cross-server connections care about idempotency and rebuild order

## 5. What reconnection really needs to solve

Reconnection is not just "connect once more", but must answer the following questions:

- Is the user still the original session
- Has the old connection already been invalidated
- What to do with messages missed during the disconnection
- Is the authoritative state still kept in memory
- Can the current state be losslessly recovered, or only partially recovered

If these questions are not designed clearly, "automatic reconnection succeeds" is only a surface phenomenon.

## 6. A more robust reconnection flow

### 1. Client side

The client usually needs to:

- Detect the disconnection
- Enter the reconnection state
- Retry with exponential backoff
- Carry the reconnection credential and the most recently confirmed sequence
- After success, request state recovery or message resend

Exponential backoff is important, because during large-scale fluctuations, immediate retries further amplify server pressure.

### 2. Server side

The server usually needs to:

- Keep short-term session context
- Invalidate the old connection
- Verify whether the reconnection credential is reusable
- Identify whether it is a legitimate recovery of the same player
- Decide which messages to resend and which to discard

The reconnection token should preferably have:

- Short validity period
- Single use
- Binding to account, session, or device information

## 7. State recovery must be leveled

Not all state deserves complete recovery. A more reasonable approach is to layer it.

### 1. State that must be recovered

For example:

- Account and character identity
- Current map or scene
- Core character attributes
- Asset state

These are usually either kept briefly in authoritative memory or can be quickly recovered from persistence.

### 2. State that should be recovered as much as possible

For example:

- Nearby entities within AOI
- The last few combat results
- Temporary UI context

These can be re-delivered or partially rebuilt after reconnection.

### 3. State for which lossless recovery is not guaranteed

For example:

- Brief animation presentation
- Non-critical prompts
- Already outdated transient broadcasts

Greatly increasing system complexity to recover such state is usually not worth it.

## 8. How to do message resend during disconnection

Resend does not mean "replay all missed packets as-is".

More feasible approaches usually fall into three categories:

### 1. Key event resend

Suits:

- System rewards
- Combat settlement
- State change notifications

### 2. Latest state rebuild

Suits:

- Character attributes
- Scene objects
- Current inventory and quest data

In many cases, directly delivering the latest authoritative state is simpler and more stable than resending historical messages.

### 3. Local hybrid strategy

For example:

- Assets and rewards use event resend
- Scene and AOI use snapshot rebuild
- Chat and prompt messages are not resent

This is usually more realistic than "resend everything in one size fits all".

## 9. Common misconceptions

### 1. The more frequent the heartbeat, the safer

No. Too-short heartbeat increases:

- Server periodic scan pressure
- Mobile power consumption
- Invalid traffic under weak networks

A reasonable heartbeat frequency must match the gameplay and platform.

### 2. Destroy player state immediately on disconnection

This directly ruins the reconnection experience. Most online games keep a short recovery window, and only do thorough cleanup after timeout.

### 3. Reconnection success equals lossless recovery

No. Link recovery and business recovery are two layers of problems. The former only means the socket is back; the latter depends on whether the state machine, cache, and resend mechanisms are complete.

### 4. All messages must be resent

Unnecessary, and often not achievable. What should really be prioritized is the correctness of authoritative state, not replaying all historical notifications once more.

## 10. A practical engineering combination

Many online games eventually settle on this combination:

- Lightweight application-layer heartbeat
- Any business message refreshes the activity time
- Server maintains lost-connection judgment and short-term session retention
- Client reconnects with exponential backoff
- Single-use reconnection token
- Key event resend plus latest state rebuild
- Assets and important operations continue to go through idempotency checks

The point of doing this is not that "the reconnection feature looks complete", but to separate link recovery, state recovery, and asset safety into three things guaranteed independently.

## References

- RFC 9293, TCP
- RFC 6455, WebSocket
- Various online game session recovery and connection keep-alive practice materials
