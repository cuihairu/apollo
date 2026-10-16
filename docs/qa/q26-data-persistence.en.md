[English](q26-data-persistence.en.md) | [中文](q26-data-persistence.md)

# Q26: When Should Player Data Be Saved to the Database? Full Save vs. Incremental Save?

## Core Conclusion

Persisting player data is not "the more often, the safer", nor "batching longer saves more database". The correct approach is usually layered saving:

- Key assets and key state are persisted in a timely manner
- Ordinary state uses dirty flags plus batch flushing
- Rebuildable temporary state is best not directly persisted

Full saving is simple to implement but has obvious write amplification; incremental saving is more efficient but more complex. Most online games end up adopting a hybrid strategy rather than either/or.

## 1. First Distinguish Which Data Deserves Immediate Saving

Not all player data should be persisted at the same time.

It can usually be divided into three categories:

### 1. High-Value Data

For example:

- Currency
- Items
- Mail attachment claims
- Transaction results
- Recharges and reward grants

This kind of data must prioritize correctness; usually it is either persisted immediately or put into a reliable persistence pipeline.

### 2. Medium-Value Data

For example:

- Quest progress
- Skill levels
- Equipment enhancement results

This kind of data commonly uses dirty flags and short-cycle batch saving.

### 3. Low-Value or Rebuildable Data

For example:

- Temporary coordinates
- Object cache within AOI
- UI state
- Certain temporary Buff presentation-layer information

This kind of data does not need to be persisted on every change.

## 2. When Is Saving More Reasonable

Common save timings are usually used in combination.

### 1. Save After Key Events

Suitable for:

- Gaining or consuming assets
- Completing key quests
- Auction deals
- Mail reward claims

This is the save point that most needs to be stable.

### 2. Periodic Batch Saving

Suitable for:

- Character growth state
- Non-critical progress
- Ordinary attributes

The advantage is smoothing database pressure; the disadvantage is that a small amount of recent changes may be lost if the service fails abnormally.

### 3. Save on Logout or Scene Switch

Suitable as a supplementary persistence point, but not as the only save timing.

The reasons are simple:

- A crash may leave no time to save
- Disconnections are unreliable in mobile network environments
- Players may stay online for a long time without logging out

### 4. Flush Before Service Shutdown

This is a fallback means, not the main save strategy.

## 3. How to Choose Between Full Save and Incremental Save

### 1. Full Save

The characteristics of full saving are:

- Simple to implement
- Clear serialization boundaries
- Easy to restore

The problems are also obvious:

- Severe write amplification
- Unrelated fields also get flushed to the database frequently
- Very costly for popular characters or highly active players

It suits:

- Small data volumes
- Smaller teams
- Prototype or early versions

### 2. Incremental Save

Incremental save only writes changed fields or changed sub-objects.

Advantages:

- Reduces write traffic
- Better suited to high-frequency changes
- Easier to split by module

Problems:

- Dirty flag maintenance is complex
- The restore path is more error-prone
- Partial state omissions are easy to occur

It is more suited to systems with large data scale, high online counts, and complex objects.

## 4. Why Most Projects End Up Going Hybrid

Because different modules have different optimal strategies.

A typical combination is:

- Periodic full snapshots of the character master record
- Inventory, currency, mail, and transactions use incremental or event-based persistence
- Logs and transaction records are written separately
- Rebuildable temporary state is not directly stored in the database

The value of doing it this way is:

- Snapshots make restoration easy
- Incremental saves reduce write amplification
- Transaction records make auditing and compensation easy

## 5. Why Saving Only in the Main Table Is Not Enough

High-value systems usually cannot rely on a single "current state table".

For currency changes, for example, the safer approach is usually:

- Update the current balance
- Record the change transaction record
- Have an idempotent ID on the critical link

Otherwise, when miswrites, retries, or compensation occur, it is very hard to trace.

## 6. A More Robust Persistence System in Engineering

A common approach is:

- Maintain the authoritative object in memory
- Use dirty flags on object fields
- Periodic tasks batch-flush ordinary dirty data
- Key asset operations are committed immediately or written to a reliable queue
- Periodically output full character snapshots
- Keep audit records for key behaviors

This way both database pressure can be controlled and restoration and reconciliation are easier.

## 7. Common Misconceptions

### 1. Writing All Changes to the Database Immediately Is Safest

This turns the database into a real-time state machine; throughput and jitter become very hard to control.

### 2. Periodic Saving Alone Is Enough

For asset-type operations it is usually not enough; the loss during a failure may be unacceptable.

### 3. Full Saving Is Simplest, Therefore Best

Simple does not equal long-term scalable. Once online volume and object complexity rise, write amplification becomes obvious.

## References

- Various online game character persistence and audit transaction record practice materials