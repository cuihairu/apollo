[English](q71-thread-vs-process.en.md) | [中文](q71-thread-vs-process.md)

# Q71: Multithreading vs multiprocess — how to choose?

## Core Conclusion

Multithreading and multiprocess have no absolute superiority. The core depends on what you value more:

- Low cost of sharing data
- Fault isolation
- Scaling boundaries
- Debugging and operations complexity

For online game services, the common conclusion is not either/or, but layered combination:

- Do responsibility isolation between processes
- Use a small number of threads inside each process as needed

## 1. The real difference between the two is not conceptual, it is about boundaries

### 1. Multithreading

Advantages usually lie in:

- Convenient shared memory
- Low cross-thread communication cost
- Suitable for collaboration within the same process

Problems are also obvious:

- Shared state brings synchronization complexity
- One severe error can bring down the whole process
- Debugging races and deadlocks is expensive

### 2. Multiprocess

Advantages usually lie in:

- Strong isolation
- Clearer failure domains
- More suitable for module splits with clear responsibility boundaries

The cost is:

- Heavier inter-process communication
- More complex deployment and observation
- Data sharing has to be designed explicitly

## 2. How to actually judge this in online games

The key is usually not "which is more advanced," but looking at the business boundary.

If a module:

- Has independent state
- Its failure should not affect other modules
- Can interact through messages

Then it is more suitable as a separate process.

If a module:

- Needs high-frequency shared data
- Strongly depends on the same address space
- Has thread collaboration cost lower than IPC

Then it may be more suitable to stay inside the process.

## 3. Why many MMO architectures lean toward multiprocess

Because many core responsibilities can naturally be layered:

- Login
- Gateway
- Scene logic
- Database proxy
- Backend services

Once split into multiple processes, it is usually easier to achieve:

- Fault isolation
- Per-module scale out
- Independent observation and deployment

That is more stable than piling all logic into one giant multithreaded process.

## 4. Multithreading still matters, but it is more suitable for local use inside a process

For example:

- I/O threads
- Background task threads
- Database worker threads
- Helper threads for compression, serialization, etc.

In other words, multithreading is better suited to solving "how to get things done more efficiently within the same process," while multiprocess is better suited to solving "how to divide system boundaries and failure domains."

## 5. Do not only look at performance; also look at fault cost

Many teams easily fixate on:

- Creation overhead
- Context switching
- IPC cost

But the more practical problems of online services also include:

- How much damage one module crashing causes
- Whether locating the problem is easy
- Whether it can be released and restarted independently

These often make multiprocess more attractive.

## 6. A more reliable engineering combination

Many mature systems end up as:

- Component-level multiprocess
- A small number of threads inside each component
- Threads avoid large-scale shared state

That gives both isolation and the efficiency of in-process collaboration.

## 7. Common misconceptions

### 1. Multithreading is definitely faster than multiprocess

Local communication may be faster, but the overall system is not necessarily more stable or more extensible.

### 2. Multiprocess is definitely more advanced

If the boundaries are not drawn well, you only get more IPC and more operations cost.

### 3. You must go all-thread or all-process

In real engineering the most common approach is a hybrid.

## References

- Multiprocess service splitting and thread model design practice materials
