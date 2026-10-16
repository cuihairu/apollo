[English](q76-async-io.en.md) | [中文](q76-async-io.md)

# Q76: How Do You Implement Asynchronous IO?

## Core Conclusion

The goal of asynchronous I/O is not "the API looks asynchronous", but keeping threads from being held pointlessly while waiting for I/O.

What truly needs to be thought clearly first is:

- What your platform provides: "readiness notification" or "completion notification"
- How the event loop is organized
- How results get back to the business logic
- Whether blocking operations are really isolated out of the main thread

Many systems claim to be asynchronous verbally, but actually just wrap blocking calls into a thread pool; this is not the same thing as a true event-driven model.

## I. Distinguish the Several I/O Modes First

The most important difference in real engineering is usually:

- Readiness notification: tells you "now you can read/write"
- Completion notification: tells you "this I/O has completed"

This difference directly affects:

- Event loop design
- Callback style
- Buffer management

## II. What Asynchronous I/O Really Solves

It mainly solves the waste of thread resources.

If a thread is blocked long-term on:

- Network receive/send
- Disk reads
- Waiting on external connections

then the concurrency cost of the system is high.

The value of asynchronous I/O is giving the waiting time away.

## III. The Event Loop Is Usually the Core

No matter whether the underlying layer is epoll, kqueue, or IOCP, engineering ultimately needs a clear event loop to handle:

- Registering events
- Waiting for events
- Dispatching events
- Driving the state machine to continue execution

Without a stable event loop, asynchronous I/O will eventually just become scattered callbacks and a complex state machine.

## IV. Asynchronous Does Not Mean No State Machine

Many people think asynchronous style can be as natural as synchronous, but the underlying layer still must handle:

- A request has been sent but not completed
- Partial reads/writes
- Timeout
- Cancellation

So whether you use callbacks, futures, or coroutines, you cannot avoid state management in essence.

## V. Network and Disk I/O Should Be Viewed Differently

Network I/O is usually more suited to event-driven; disk I/O differs much more across platforms in how it is supported.

So when implementing asynchronous I/O, do not simply assume "all I/O can be handled uniformly by the same abstraction".

## VI. Business Logic Should Not Couple Directly to Low-Level Event Details

A safer approach is usually:

- The low-level I/O layer handles reading/writing and event registration
- The upper protocol layer handles packet assembly and disassembly
- The business layer only receives more stable message events

This way the asynchronous complexity does not leak all the way into business code.

## VII. Safer Engineering-Wise Practice

Common practice is:

- Use an event loop to drive network I/O
- Make connection state machines explicit
- Isolate truly blocking external calls separately
- Dispatch results back uniformly to a stable business execution context

This way the system is both asynchronous and maintainable.

## VIII. Common Misconceptions

### 1. Using Callbacks Means Asynchronous I/O

Not necessarily. The underlying layer may still be a thread pool wrapping blocking calls.

### 2. Asynchronous I/O Automatically Makes the System Faster

It mainly improves resource utilization; it does not automatically eliminate business logic and serialization costs.

### 3. Asynchronous Code Does Not Need Thread Model Design

On the contrary, the boundary between the event loop and business threads must be clearer.

## References

- Materials on Reactor, Proactor, and event loop implementations
