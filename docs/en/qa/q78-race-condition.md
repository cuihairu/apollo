
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q78: How Do You Handle Race Conditions?

## Core Conclusion

The essence of a race condition is not "things get messy when there are many threads", but that the read/write order of multiple execution paths over the same piece of state is no longer controllable.

The most effective handling is usually not bolting on locks afterward, but first reducing shared state, clarifying state ownership, and giving key modifications an atomic boundary.

## 1. What a Race Condition Really Is

As long as the program result depends on the order of execution, and that order is not controlled, a race can occur.

It does not only happen in:

- Multithreading

It can also happen in:

- Asynchronous callbacks
- Timers interleaving with the main flow
- Duplicate requests across services

So a race is a "state timing problem", not just a "thread problem".

## 2. The Most Common Sources of Races

For example:

- Check-then-modify, where the intermediate state has already changed
- The same object being written by multiple threads at the same time
- Timeout retries and the original request arriving concurrently
- Callback results returning later than the object's lifetime

These are closer to real online problems than the textbook `counter++`.

## 3. The Most Effective First Step Is Usually Reducing Sharing

If a piece of state can be clearly owned by:

- A certain thread
- A certain Actor
- A certain session or instance

Then many races naturally disappear.

This is usually more stable than constantly bolting locks onto shared objects.

## 4. The Atomic Boundary Must Be Clear

Many races come from:

- Reading an old value
- Computing a new value
- Writing it back when it is already stale

So key updates usually need:

- Atomic operations
- Lock protection
- Version numbers
- Compare-and-swap

Which to choose specifically depends on the sharing pattern and complexity.

## 5. The Check-Then-Use Gap Is the Most Prone to Problems

This is a very typical class of race in real engineering:

- First check "the object exists"
- The next line uses it, but the object has already been destroyed or moved

So what is often really needed is:

- Lifetime binding
- Version validation
- A stable reference for the holding period

Not just one more lock.

## 6. Races in Asynchronous Systems Are Often More Hidden

For example:

- An RPC that times out and then returns late
- A callback still arriving after the timer is cancelled
- A player going offline and the database async save returning only then

Common handling for these problems includes:

- Request IDs
- Version numbers
- State machine validation
- Invalidation flags

## 7. The More Robust Handling in Engineering

Common practice is:

- State has a single owner as much as possible
- Multi-step modifications are compressed into an atomic boundary
- Asynchronous callbacks carry a context ID or version
- Key object lifetimes are explicitly managed

This is usually more stable than "bolting a lock wherever a problem appears".

## 8. Common Misconceptions

### 1. Race Condition Is the Same as Data Race

A data race is only one kind. Many business races happen between asynchronous links and state machines.

### 2. Locking Solves All Races

Not necessarily. Lifecycle, duplicate requests, and asynchronous callback problems often need state machines and version control.

### 3. Races Only Happen Under High Concurrency

They can also happen under low concurrency; they are just harder to reproduce.

## References

- Race conditions, version control, and asynchronous state machine practice materials
