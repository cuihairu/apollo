[English](q83-speed-hack.en.md) | [中文](q83-speed-hack.md)

# Q83: How to prevent speed hacks?

## Core Conclusion

The essence of a speed hack is not "the client's clock runs faster," but the client trying to make its own action progression exceed the upper limit allowed by the server.

Truly effective protection usually relies on:

- Authoritative server time and movement constraints
- Input frequency and cooldown validation
- Anomalous behavior detection

Relying only on the client's local clock or local frame rate cannot stop a real speed hack.

## 1. What a speed hack actually affects

Common manifestations include:

- Abnormal movement speed
- Abnormal attack frequency
- Shortened skill cooldowns
- Shortened gathering and interaction times

So a speed hack is not a single problem. It is a general term for "time-related logic being illegally advanced."

## 2. The server must own the final time semantics

For example:

- Movement step length advances by server tick or authoritative time
- Skill cooldowns are recorded by the server with start and end
- Gathering progress is advanced by the server

The client can present more smoothly, but it cannot decide the final time boundary.

## 3. Movement-type speed hacks are the most common, and also the easiest to authoritatively validate

The core idea is usually:

- Record the last authoritative position and time
- Calculate the theoretical maximum movable distance over that interval
- When the threshold is exceeded, reject, correct, or score it

Here you have to consider these factors together:

- Teleports and dash skills
- Knockback or displacement effects
- Position catch-up packets caused by network jitter

Otherwise you easily hurt normal players.

## 4. Attack and skill-type speed hacks are more like "frequency anomalies"

This kind of detection usually focuses more on:

- Skill cast intervals
- Normal attack frequency
- Continuous input rhythm

In other words, the server has to know:

- When a skill entered cooldown
- The earliest time the next legal cast can happen

As long as these times are all adjudicated by the server, the client can hardly break through just by changing its local speed.

## 5. Do not treat every anomaly as a cheat and ban immediately

A more reliable approach is usually:

- First anomaly: correct or discard
- Persistent anomaly: score it
- Punish only after multi-dimensional anomalies stack up

Because network anomalies, client stutter recovery, and prediction correction can all produce edge errors.

## 6. A more reliable engineering combination

A common approach is:

- The server validates the maximum progression speed of positions and actions
- Cooldowns and interaction times are recorded by the server
- Anomalous behavior is continuously accumulated statistically rather than judged in a single shot
- Key anomalies keep logs for review

## 7. Common misconceptions

### 1. Just checking the client timestamp is enough

Not enough. The client's time is inherently untrustworthy.

### 2. Banning on the first speed anomaly is the most reliable

It easily hurts players, especially on weak networks and during reconnects.

### 3. Speed hacks only affect movement

Attack frequency, skill cooldowns, and interaction times are equally important.

## References

- Online game speed anomaly detection and authoritative server time design practice materials
