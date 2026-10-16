[English](q72-avoid-deadlock.en.md) | [中文](q72-avoid-deadlock.md)

# Q72: How to Avoid Deadlocks?

## Core Conclusion

The most effective way to avoid deadlocks is not after-the-fact detection, but reducing multi-lock sharing and inconsistent resource acquisition order from the very beginning.

A more pragmatic set of principles is usually:

- Don't share what doesn't need to be shared
- If something can be owned by a single thread, don't have multiple threads fight over it
- When locks are mandatory, unify the order
- Compress lock hold time to the shortest

A deadlock is essentially a system-boundary and resource-acquisition-order design problem, not just a code-technique problem.

## I. How Deadlocks Really Happen

The most common deadlock scenario is usually not a complex algorithm, but:

- Thread A holds lock 1, waiting for lock 2
- Thread B holds lock 2, waiting for lock 1

Once the resource acquisition order is inconsistent, deadlocks easily appear.

So what matters more than memorizing the four necessary conditions is: first find which operations in the system may hold multiple locks at the same time.

## II. The Most Effective Prevention Is Usually Reducing Sharing

If an object can naturally be attributed to:

- A certain thread
- A certain Actor
- A certain logic partition

Then avoid having multiple threads modify it simultaneously.

This is a more stable approach than "designing a complex lock protocol".

## III. When Multiple Locks Are Mandatory, the Order Must Be Unified

This is one of the most classic and most practical rules.

For example:

- Always lock in ascending object ID order
- Always take the account lock first, then the order lock

As long as the order is unified, the probability of circular waiting drops substantially.

## IV. Lock Scope Should Be as Small as Possible

Many deadlocks are not because there are too many locks, but because:

- Locks are held too long
- I/O is done while holding the lock
- External modules are called while holding the lock

This significantly enlarges the deadlock window.

A more prudent approach is usually:

- First collect the necessary data
- Then hold the lock for the minimal modification
- Do subsequent logic after releasing the lock

## V. Avoid Calling Uncontrollable Logic Inside a Lock

For example:

- Callbacks
- RPC
- Database access
- Complex script execution

You usually cannot guarantee these call chains won't take other locks internally, and they most easily create hidden deadlocks.

## VI. Detection and Timeout Are Supplements, Not the Main Plan

Deadlock detection, lock timeouts, and watchdogs are all valuable, but they are more like stop-the-bleeding measures.

The real main plan should still be:

- Unified order
- Reduced sharing
- Shrunk critical sections

## VII. More Pragmatic Design Habits in Engineering

Common habits include:

- Prescribing a fixed order for multi-lock operations
- Avoiding lock nesting where possible
- Exposing few internal locks at the common facility layer
- Doing tracing and wait statistics on hotspot locks

This way problems can be found before they become production incidents.

## VIII. Common Misconceptions

### 1. Using `std::scoped_lock` Means No Deadlocks

It helps in some scenarios, but it cannot replace system-level resource-order design.

### 2. Deadlocks Only Appear in Complex Code

In reality many deadlocks come from very ordinary two-object operations.

### 3. Once a Deadlock Is Found, Just Add Retries

Retries can alleviate some problems, but they cannot replace fixing the root cause.

## References

- Materials on concurrent deadlock prevention, lock ordering, and tracing practices
