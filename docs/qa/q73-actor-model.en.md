[English](q73-actor-model.en.md) | [中文](q73-actor-model.md)

# Q73: What Is the Actor Model? What Advantages Does It Have?

## Core Conclusion

The core value of the Actor model is not the "message queue" form, but changing shared state into "single owner plus message driven".

The problems it is best suited to solve are usually:

- Too much shared state
- Obvious lock contention
- Objects that can naturally be partitioned by entity, room, or scene

But the Actor model is not a universal silver bullet; cross-Actor collaboration, mailbox backlog, and debugging complexity are all real costs.

## 1. What the Actor Model Is Actually Doing

You can understand an Actor as:

- A piece of state exclusively owned by it
- An input mailbox
- A piece of logic that serially processes those messages

Its key is not "asynchrony", but "the same piece of state is modified by as few execution contexts as possible".

## 2. Why It Can Reduce Lock Problems

Because it tries to avoid multiple threads modifying the same object at the same time.

If objects like players, rooms, and scenes each have a clear owner, then many shared writes that originally needed locks become message delivery.

This is also the biggest engineering value of Actors.

## 3. Where It Is Most Natural in Game Services

The most common placements include:

- Player entities
- Rooms
- Instance partitions
- Scene partitions

These objects are naturally suited to:

- Serially advancing state
- Receiving external requests via messages

## 4. Where the Advantages of Actors Mainly Manifest

### 1. Clear State Ownership

It is easier to say clearly who can modify what state.

### 2. Less Lock Contention

Once many shared writes become message-based, hotspot locks decrease significantly.

### 3. More Suited to Distributed Scaling

If Actor identity and location are routable, it naturally has the chance to scale to multiple nodes.

## 5. Actors Also Have Obvious Costs

### 1. Mailbox Backlog

If a single Actor becomes a hotspot, it becomes a serial bottleneck.

### 2. Cross-Actor Coordination Is Complex

Once a business needs to modify the state of multiple Actors at the same time, transactions and consistency become obviously complex.

### 3. Higher Debugging Difficulty

Asynchronous message chains are usually harder to trace than direct calls.

## 6. Do Not Call Every Message Queue System an Actor

The premise truly worth calling Actor style is usually:

- State has a clear owner
- Processing is serial
- Externals modify state via messages rather than shared memory

If it is just "thread pool plus queue", that does not equal the Actor model.

## 7. The More Robust Understanding in Engineering

The Actor model is more like a concurrency organization method, not a belief that must be carried through globally.

Many online games adopt Actor ideas locally, for example:

- Serial processing of entities within a scene
- Room messages advanced by a single thread

But the peripheral systems may still be:

- Ordinary RPC
- Multi-process services
- Thread pool background tasks

## 8. Common Misconceptions

### 1. Once You Use Actors, There Are No Concurrency Problems

No. Cross-Actor coordination and hotspot Actors equally become problems.

### 2. Actors Are Definitely Faster Than Locks

Not necessarily. It depends on message cost, granularity, and the actual load model.

### 3. All Modules Are Suited to Actor-ification

Many backend management and low-frequency services are not worth forcing into Actors.

## References

- Actor model, mailbox scheduling, and game entity concurrency organization practice materials