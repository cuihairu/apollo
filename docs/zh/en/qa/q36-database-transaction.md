
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q36: How to Handle Database Transactions?

## Core Conclusion

The value of a transaction is not "wrapping everything up," but guaranteeing, within a necessary boundary, that a set of modifications either all succeed or all fail.

In online games, the most important thing is not memorizing ACID, but first judging:

- Whether this call path really needs a transaction
- Whether the transaction boundary can be kept near a single database and single object
- Whether timeout, retry, idempotency, and audit are designed at the same time

Many system incidents are not caused by the absence of transactions, but by using transactions in the wrong place, or wrongly expecting transactions to solve all consistency problems.

## I. Which Scenarios Especially Need Transactions

Usually these include:

- Deducting money and granting items
- Claiming mail attachments and marking them claimed
- Multi-table updates at trade confirmation
- State transitions at auction completion

These operations share a common trait:

- The result must be atomic
- Intermediate state must not be exposed

## II. The Smaller the Transaction Boundary, the Better

Bigger is not more stable for transactions; often the opposite is true.

Common problems with large transactions:

- Locks held for a long time
- High conflict probability
- High rollback cost
- Large blast radius on failure

A more robust principle is usually:

- Wrap only the necessary SQL
- Try not to put external RPC, network waits, or complex business computation inside the transaction

## III. Local Transactions and Distributed Transactions Are Not the Same Thing

### 1. Local Transactions

Suitable for a set of updates within a single database; the most common and most reliable transaction form.

### 2. Distributed Transactions

Cross-service, cross-database, and cross-queue scenarios are far more complex.

Many online games do not use distributed transactions heavily on main paths, but instead prefer:

- Single authoritative writes
- Idempotency
- Event-driven design
- Compensation

Because forcing all cross-service paths into distributed transactions is usually very costly.

## IV. Isolation Level Should Be Chosen by Conflict Model

Higher isolation is not always better.

Higher isolation usually means:

- Lower throughput
- More conflicts
- Higher latency

The actual choice depends on:

- Whether concurrent deduction can occur
- Whether range queries exist
- Whether phantom reads are a concern

Many businesses do not need the highest isolation level, but must pair it with the correct update method, such as conditional updates, version numbers, or row locks.

## V. Deadlocks Are a Design Problem, Not Just a Database Problem

Common causes of deadlock include:

- Multiple transactions accessing resources in inconsistent order
- Large transactions holding locks too long
- One transaction updating too many objects

Common governance measures:

- Fixed access order
- Narrower transaction scope
- Bounded retry after failure
- Monitoring and slow-transaction investigation

## VI. Transactions and Idempotency Must Be Considered Together

A transaction guarantees the atomicity of a set of database modifications; it does not guarantee "external retries won't execute repeatedly."

For example, when a client times out and retries, even if the first transaction already succeeded, the second request may still enter again.

So key write operations usually also need:

- A unique request ID
- Idempotency protection
- A journal/ledger record

## VII. A More Robust Engineering Approach

Many core paths adopt:

- Local transactions for key steps within a single database
- Event-driven design for cross-service modifications
- Idempotency at the outer request level
- Journal records kept for key results

This keeps transactions at the boundary they are best at, rather than abusing them.

## VIII. Common Misconceptions

### 1. All write operations should open a transaction

Wrong. Transactions have a cost; many single-statement atomic updates do not need an extra large transaction.

### 2. Transactions can solve cross-service consistency

Single-database transactions cannot; cross-service scenarios need additional mechanisms.

### 3. The highest isolation level is the safest

In many cases it is just slower and more conflict-prone, and not necessarily better suited to the business.

## References

- MySQL / InnoDB transaction and isolation level materials
- Local transaction and idempotency practice materials for various online game asset paths
