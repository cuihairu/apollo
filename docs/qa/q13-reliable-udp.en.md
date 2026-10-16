[English](q13-reliable-udp.en.md) | [中文](q13-reliable-udp.md)

# Q13: What Is Reliable UDP? How to Implement It?

## Core Conclusion

"Reliable UDP" is not a fixed protocol name, but a class of approach:

- The underlying layer still uses UDP to send datagrams
- But the application layer adds part of the reliable transmission capability itself

Its value is not in "completely replacing TCP", but in:

- Adding reliability only to messages that truly need it
- Preserving UDP's lighter, more flexible transmission semantics
- Avoiding stuffing all messages into TCP's single reliable byte stream

So the real key question is not "whether reliable UDP can be done", but:

- Why add reliability yourself
- To what degree
- Which messages deserve reliability
- Which semantics cannot be copied from TCP

---

## 1. Why Reliable UDP Is Needed

If you only look at the two extreme choices:

- TCP: reliable and ordered, but heavy semantics
- UDP: lightweight and flexible, but unreliable by default

Then many real-time systems fall in the middle ground:

- Some messages cannot be lost
- But you don't want all messages held back by TCP's head-of-line blocking and unified retransmission strategy

Typical examples:

- Skill cast requests should not be easily lost
- Important combat events had better arrive reliably
- But high-frequency position states are not worth retransmitting one by one

At this point an idea emerges:

Implement "selective reliability" on top of UDP yourself.

---

## 2. What Reliable UDP Solves Is Not "Everything Reliable", but "Reliability on Demand"

This is the most easily misstated point.

The goal of reliable UDP is usually not to replicate a complete TCP, but to:

- Add ACK and retransmission for key messages
- Continue allowing old state messages to be dropped
- Do independent ordering for streams with high ordering requirements
- Give different transmission semantics for different message types

In other words, the value of reliable UDP often comes from "message tiering".

If you put all messages into the same reliable UDP channel, you will most likely just reinvent a harder-to-maintain TCP.

---

## 3. The Most Basic Mechanisms a Reliable UDP Needs to Add

### 3.1 Sequence Numbers

To know the order of packets, you first need sequence numbers.

Its roles include:

- Detecting packet loss
- Detecting out-of-order delivery
- Detecting duplicates

Without sequence numbers, there is no reliability control to speak of.

### 3.2 Acknowledgement Mechanism

The sender must know which packets have arrived.

Common approaches:

- ACK: acknowledge receipt
- NACK: explicitly state that certain packets were not received

ACK is the core feedback channel of reliable UDP.

### 3.3 Retransmission Mechanism

If a key message has not arrived, you must decide whether to resend it.

But here you must pay close attention:

- Not all messages are worth retransmitting
- Retransmitting messages past their useful lifetime is pointless

So reliable UDP retransmission is usually "conditional retransmission", not "blind retransmission".

### 3.4 Deduplication

Once there is retransmission, duplicate arrival is inevitable.

So the receiver must be able to identify:

- Whether this packet is arriving for the first time
- Or was already processed before

Otherwise the same skill or the same trade event may be executed multiple times.

### 3.5 Out-of-Order Handling

UDP does not guarantee ordering.

So if certain message streams require ordering, you must decide yourself:

- Whether to buffer and wait for missing packets
- Or skip old packets
- Or directly overwrite with the latest state

This step cannot be handled uniformly; it must be decided per message type.

---

## 4. The Most Critical Thing About Reliable UDP Is Not "Adding Mechanisms", but "Not Overdoing It"

The biggest problem with many self-developed reliable UDPs is not insufficient functionality, but adding so much that it becomes too much like TCP.

### 4.1 Typical Over-Engineering

- Forcing reliability on all packets
- Forcing ordering on all packets
- All lost packets must wait for retransmission
- All streams share one blocking window

The consequences are usually:

- Losing UDP's flexibility
- Reproducing TCP's blocking problems
- Higher implementation complexity
- Without necessarily better results

### 4.2 A More Reasonable Approach

More mature reliable UDPs are usually split into multiple message semantics:

- Reliable event streams
- Unreliable state streams
- State snapshot streams that drop old and keep new

Only then can it truly deliver its value.

---

## 5. A More Practical Implementation Approach

### 5.1 First Split Messages into Three Categories

#### Category 1: Reliable Events

For example:

- Skill casts
- Damage events
- Key state transitions
- Important control events in combat

Characteristics:

- Cannot be casually dropped
- Retransmission is meaningful
- Usually need idempotent handling

#### Category 2: Unreliable High-Frequency State

For example:

- Position
- Facing direction
- High-frequency movement state

Characteristics:

- Old messages do not matter
- Not worth retransmitting
- Only the latest state needs to arrive as soon as possible

#### Category 3: Semi-Reliable State

For example:

- Medium-frequency state snapshots
- State that needs rough consistency, but where old packets quickly lose value

These messages can:

- Undergo a limited number of retransmissions
- Be abandoned after timeout

### 5.2 Choose Different Handling Strategies per Message Type

A truly mature implementation is usually not "one reliable UDP rule handling all messages", but:

- Reliable event channel: ACK + retransmission + deduplication
- High-frequency state channel: no retransmission, overwrite with latest
- Semi-reliable channel: limited retransmission, drop on timeout

---

## 6. The Biggest Semantic Difference Between Reliable UDP and TCP

### 6.1 TCP's Default Semantics

What TCP does for you by default is:

- The entire byte stream is reliable
- The entire byte stream is ordered
- Later content usually has to wait for earlier content to be filled in first

### 6.2 Semantics Reliable UDP Is Better Suited For

The more common goal of reliable UDP is:

- Some messages reliable
- Some messages unreliable
- Some messages can tolerate out-of-order delivery
- Some messages expire and become invalid

This is the biggest difference between the two.

So the advantage of reliable UDP is not "being more reliable than TCP", but:

It allows you to separate the reliability semantics of different messages.

---

## 7. Several Points Most Easily Overlooked During Implementation

### 7.1 Idempotence

As long as there is retransmission, duplicate execution must be considered.

So key events must be designed to be:

- Either naturally idempotent
- Or carry an event ID for deduplication

Otherwise "reliability" will only amplify errors.

### 7.2 Whether Timed-Out Messages Are Still Worth Retransmitting

Some messages are meaningful within 50ms, but meaningless after 200ms.

For example:

- Outdated position state
- Outdated action presentation

Even if such messages are lost, they should not be retransmitted.

### 7.3 Send Window and Congestion

If you only add ACK and retransmission but no rate limiting or window control, on a poor network you will only:

- Lose more and retransmit more
- Retransmit more and congest more
- Make the overall effect worse

So even without full TCP congestion control, you should at least have:

- A basic window
- Basic throttling
- Basic timeout backoff

### 7.4 Observability

A self-developed reliable UDP without observability is almost unmaintainable.

At minimum you must be able to see:

- RTT
- Packet loss rate
- Retransmission rate
- Duplicate packet rate
- Out-of-order rate
- Window occupancy

Otherwise, when problems occur online, you will only see "players say it's laggy" but not know which layer the lag is in.

---

## 8. When Reliable UDP Is Worth Doing

### 8.1 Situations Where It Is Worth Doing

- Real-time interaction is clearly limited by TCP semantics
- It is already clear that reliable events and unreliable state need to be distinguished
- The team is capable of maintaining transport-layer logic long-term
- The project genuinely needs a better combat experience on weak networks

### 8.2 Situations Where It Is Not Worth Rushing Into

- Messages have not been tiered yet
- The current bottleneck is actually in business threads, AOI, broadcasting, or serialization
- The team has no network protocol maintenance experience
- The project's main business is actually dominated by reliable state management

In this situation, blindly adopting reliable UDP easily moves the problem from one place to another.

---

## 9. The Relationship with KCP

KCP can be seen as a very mature "reliable UDP approach implementation".

It already adds many basic capabilities for you:

- Sequence numbers
- ACK
- Retransmission
- Window
- RTT estimation

So the real choice for many projects is not:

- "Whether to have reliable UDP"

But:

- "Build one yourself, or directly use a mature solution like KCP"

In most cases, if you only need a working reliable UDP mechanism, prioritizing reuse of a mature implementation is more stable.

---

## 10. Summary

The essence of reliable UDP is not "turning UDP into TCP", but:

- Preserving UDP's flexibility
- Adding reliability only to the messages that need it
- Avoiding making all messages share the same retransmission and blocking semantics

A more sound engineering practice is usually:

- Tier the messages first
- Then decide which messages need reliable UDP
- Finally decide whether to develop it yourself or directly use a mature implementation

If message tiering is not yet clear and you directly discuss the implementation details of reliable UDP, you will usually make the system too heavy.

---

## References

- [KCP](https://github.com/skywind3000/kcp)
- [KBEngine GitHub - reliable_udp](https://github.com/kbengine/kbengine/blob/master/kbe/src/server/network/reliable_udp.h)
- [Selective Acknowledgment](https://datatracker.ietf.org/doc/html/rfc2018)
