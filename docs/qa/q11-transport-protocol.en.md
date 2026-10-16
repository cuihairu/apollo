[English](q11-transport-protocol.en.md) | [中文](q11-transport-protocol.md)

# Q11: TCP vs UDP vs KCP — what are their respective use cases in MMOs?

## Core Conclusion

This is not a question of "which protocol is more advanced", but of "which type of message should go over which transport semantics".

- TCP suits messages where reliability comes first, ordering comes first, and implementation cost is a concern
- UDP suits high-frequency, droppable messages where the latest state overwrites the old state
- KCP suits scenarios where you want to keep UDP's low-latency characteristics while adding reliability at the application layer

The thing you should actually do first is not pick a protocol, but layer your messages first:

- Messages that absolutely cannot be lost
- Messages that can be retried
- Messages where dropping the old and keeping the new is acceptable
- High-frequency real-time state messages

---

## One: Don't Ask "Which Protocol" First — Ask "What Is the Message" First

In an MMO, different messages have very different requirements.

For example:

- A login request cannot be casually dropped
- An inventory modification cannot arrive out of order
- A transaction result must not be credited twice
- Position synchronization allows old packets to be dropped
- Voice messages tolerate minor corruption

So the real order of judgment should be:

1. Must this message arrive reliably?
2. Must this message be processed in order?
3. If it is lost, is retrying more reasonable, or is ignoring it more reasonable?
4. Is it a state message or an event message?

If this step is not sorted out first, any later discussion of TCP, UDP, and KCP tends to become empty talk.

---

## Two: What the Three Protocols Essentially Solve

### 2.1 TCP

TCP provides:

- Connection-oriented
- Byte stream
- Reliable transmission
- In-order delivery
- Congestion control
- Flow control

Its biggest advantage is not that it is "fast", but that:

the application layer does not have to handle too many reliability/delivery problems itself.

### 2.2 UDP

UDP provides:

- Connectionless
- Datagram
- Best-effort transmission
- No delivery guarantee
- No ordering guarantee
- No retransmission guarantee

Its biggest advantage is not "universally low latency", but that:

the transport semantics are very light, and the application layer can fully customize them to its own business needs.

### 2.3 KCP

KCP is not a new fourth-layer protocol, but:

- Carried over UDP
- Implements reliable transmission and retransmission strategies at the application layer
- Aims to pursue low-latency recovery more aggressively than TCP

Its essence is:

you are willing to spend more bandwidth and implementation complexity in exchange for better interactive latency under poor network conditions.

That does not mean it is naturally suited to all MMO messages.

---

## Three: What Should Really Be Compared Is Not the "Protocol Name" but the Transport Semantics

### 3.1 TCP's Advantages and Costs

Advantages:

- Reliable
- Ordered
- Universal
- Best compatibility with network equipment
- Relatively low development and troubleshooting cost

Costs:

- Has head-of-line blocking problems
- When one packet gets stuck, the subsequent byte stream is also blocked
- Unfriendly to high-frequency state synchronization
- If you do not actually need reliability and strict ordering, it does a lot of unnecessary work on your behalf

### 3.2 UDP's Advantages and Costs

Advantages:

- Low overhead
- Simple send semantics
- No TCP-style byte-stream blocking chain
- Very well suited to "latest state overwrites old state" messages

Costs:

- You have to implement all reliability yourself
- You have to implement all ordering control yourself
- Retransmission, congestion, rate limiting, and anti-cheat all become harder
- Once the design is poor, it is easy to move the problem from the kernel layer up to the business layer

### 3.3 KCP's Advantages and Costs

Advantages:

- Recovery speed under poor networks is usually more aggressive than TCP
- Friendlier to real-time interactive messages
- You can tune the window, flush interval, and retransmission strategy yourself

Costs:

- Higher bandwidth overhead
- Higher implementation complexity
- Poorly tuned parameters can make things worse
- Higher operational and troubleshooting complexity than pure TCP

So KCP's keyword is not "stronger", but "more tunable, and easier to misuse".

---

## Four: How Messages Should Actually Be Layered in an MMO

Rather than dividing by system name, it is better to divide by message semantics first.

### 4.1 Strongly Reliable Messages

Characteristics:

- Loss is unacceptable
- High risk of duplicate execution
- Ordering usually matters

Typical examples:

- Login authentication
- Character creation
- Inventory modification
- Item granting
- Trading
- Payment results
- Mail claiming

For this kind of message, prefer TCP, or at least have reliable delivery semantics equivalent to TCP.

### 4.2 Reliable Event Messages

Characteristics:

- They are discrete events
- They usually cannot simply be dropped
- But they also have some requirement for absolute minimum latency

Typical examples:

- Skill cast requests
- Skill hit confirmations
- Combat settlement events
- Important state transitions

This kind of message sometimes suits:

- TCP
- Or UDP + KCP / an application-layer reliable event channel

It depends on the game's pace and poor-network goals.

### 4.3 High-Frequency State Messages

Characteristics:

- High update frequency
- Old messages quickly lose value
- You care more about the latest state than about every state arriving

Typical examples:

- Position synchronization
- Facing synchronization
- Action state broadcast
- Short-cycle AOI state

This kind of message naturally suits UDP semantics better than forcing per-message reliable delivery.

### 4.4 Media Streams or Weakly Reliable Messages

Typical examples:

- Voice
- Some spectator streams
- Certain low-value presentation-layer events

This kind of message tolerates partial loss; timeliness is the priority.

---

## Five: Why Many MMO Backbones Still Lean Toward TCP

Many people default to "real-time games must use UDP/KCP" the moment game networking comes up, and that is not always true for MMOs.

The reasons are simple:

- An MMO's core business contains many reliable messages
- Many systems are state management, not pure operation streams
- The game pace usually does not depend on millisecond-level input feedback the way an FPS does
- Implementation complexity and operational cost matter a lot in MMOs

So for a large number of MMO projects:

- Login
- Inventory
- Trading
- Social
- Most quest and growth systems

Using TCP for these core paths is completely reasonable.

Some MMOs even choose "TCP overall + client-side interpolation compensation" instead of jumping straight to a complex hybrid protocol stack.

---

## Six: When UDP / KCP Are Actually Valuable

### 6.1 Scenarios Where UDP Is More Valuable

- High-frequency position synchronization
- High-frequency posture synchronization
- Voice
- State streams where dropping the old and keeping the new is acceptable

The core here is not "faster", but:

you simply do not want to do reliable retransmission for old state.

### 6.2 Scenarios Where KCP Is More Valuable

- Skill casting and combat operations are latency-sensitive
- Poor-network quality fluctuates noticeably
- You want faster packet-loss recovery than TCP
- The team has the ability to maintain and tune an extra transport layer

If the team cannot maintain this complexity long-term, KCP's benefit may not exceed its cost.

---

## Seven: Several Common Misconceptions

### 7.1 Misconception One: UDP Is Always Better for Games

No.

UDP only means:

- You get more raw transport semantics
- But reliability, ordering, rate limiting, and congestion all have to be added by you

If most of your messages must be reliable in the first place, UDP is not necessarily more suitable.

### 7.2 Misconception Two: KCP Is Always Better Than TCP

No.

KCP only fits the goal better than TCP in certain poor-network, real-time interaction scenarios.

But the extra problems it brings include:

- More bandwidth
- More parameters
- More complex troubleshooting
- Higher client and server maintenance cost

### 7.3 Misconception Three: A Project Can Only Choose One Protocol

Also no.

A more mature approach is often to mix and match semantics by message type; and even if only one physical connection is exposed externally, the interior will still do multi-channel message grading.

---

## Eight: A More Practical Selection Method

### 8.1 If You Prioritize Stability and Landing

Consider first:

- TCP for the main path
- Optimize the experience through message grading, client-side interpolation, prediction, and compression

This is usually the most stable route for small and medium teams.

### 8.2 If You Already Have a Clear High-Frequency Real-Time Bottleneck

Then consider:

- Splitting the high-frequency state stream onto UDP separately
- Or putting real-time combat messages on a KCP / application-layer reliable UDP channel

At this point you must first prove:

- The problem is really caused by TCP transport semantics
- And not serialization, broadcast, AOI, main-thread processing, or client presentation-layer problems

### 8.3 A Typical Real-World Combination

A fairly common approach is:

- TCP handles login, inventory, social, quests, and assets
- UDP or KCP handles position, actions, and real-time combat state

But whether to go this far depends on the project type and team capability; it is not the default answer.

---

## Nine: Summary

The choice among TCP, UDP, and KCP is essentially not a protocol preference, but a message-semantics choice.

For MMOs:

- Strongly reliable, strongly consistent messages prefer TCP
- High-frequency state synchronization suits UDP semantics better
- Consider KCP only when there is a clear need for low-latency reliable interaction

If you have not layered your messages first, then just discussing "should the project use TCP or UDP/KCP" usually does not lead to a solid conclusion.

---

## References

- [KBEngine GitHub - Network](https://github.com/kbengine/kbengine/tree/master/kbe/src/server/network)
- [KCP](https://github.com/skywind3000/kcp)
- [TCP](https://en.wikipedia.org/wiki/Transmission_Control_Protocol)
- [UDP](https://en.wikipedia.org/wiki/User_Datagram_Protocol)
