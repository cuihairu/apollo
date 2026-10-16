[English](q60-timer-system.en.md) | [中文](q60-timer-system.md)

# Q60: How to Design a Timer System?

## Core Conclusion

A timer system is not as simple as "callback when the time comes"; it is essentially time scheduling among a large number of delayed tasks.

What really needs to be clarified first is:

- Is the goal precision or throughput
- How large is the scale of timers
- Whether callback execution is allowed to block the main loop
- How cancellation and repeated scheduling are handled

If these boundaries are unclear, timers quickly become a source of production jitter and performance problems.

## I. What a Timer System Really Handles

Common uses include:

- Skill cooldowns
- Periodic Buff triggering
- Connection timeouts
- Instance countdowns
- Scheduled tasks

This means the timer system serves many upper-layer modules, so it must be stable enough.

## II. First Distinguish Several Types of Timers

### 1. One-shot Timers

Fire once and then end, for example:

- Timeout detection
- Delayed execution

### 2. Repeating Timers

Execute repeatedly at a fixed interval, for example:

- Heartbeat checks
- Periodic Buffs

### 3. Logic-frame Timers

Advance by game tick rather than being triggered by wall-clock time.

This is very common when alignment with combat, skills, and state machines is needed.

## III. Time Precision and Performance Must Be Weighed

Not all timers need millisecond-level precision.

For example:

- Skill cooldowns and combat events may need finer granularity
- Daily cleanup and offline reclamation do not need it at all

If all timers use the same high-precision structure, the cost will be very high.

## IV. How to Choose a Common Data Structure

### 1. Min-heap

Suitable for:

- Medium counts
- Need for relatively high precision

The advantage is clear semantics; the downside is that neither insertion nor deletion is constant-time.

### 2. Time Wheel

Suitable for:

- Large timer counts
- Relatively controllable precision requirements

Many online systems ultimately choose a time wheel or hierarchical time wheel to carry a large number of scheduled tasks.

There is no absolutely best; the key is the scale and precision requirement.

## V. Callback Execution Must Not Drag the Scheduler Down

One of the most common real problems with timer systems is not inaccurate time, but callbacks that are too heavy.

A more prudent approach is usually:

- The scheduler is only responsible for discovering tasks that are due
- Actual callbacks are handed to the logic thread or a task queue

Otherwise one slow callback will delay all subsequent timers.

## VI. Cancellation and Repeated Triggering Require Special Care

Timers often encounter:

- Being cancelled before the callback
- Being registered again while the callback is executing
- The object has already been destroyed but the timer still exists

So the timer system must have explicit lifecycle constraints and invalidation checks.

## VII. A More Pragmatic Design in Engineering

Common practice is:

- Split different precision tiers by use case
- Separate the scheduling structure from the execution structure
- Bind timers to an owner or context
- Use lazy deletion or invalidation flags for cancellation

This makes it easier to balance count, precision, and safety.

## VIII. Common Misconceptions

### 1. The More Precise a Timer, the Better

No. Many tasks are simply not worth the high cost of extreme precision.

### 2. Simply Executing the Callback Directly When the Time Comes Is Simplest

Simple, but it easily blocks the entire scheduling chain.

### 3. One Global Timer Manager Is Enough

Once the scale grows, tiering by use case is often more reasonable.

## References

- Materials on time wheels, min-heaps, and large-scale timer scheduling
