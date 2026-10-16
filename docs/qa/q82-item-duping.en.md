[English](q82-item-duping.en.md) | [中文](q82-item-duping.md)

# Q82: How to Prevent Item Duping?

## Core Conclusion

The essence of the item duping problem is not "an extra copy of an item", but that the system allows the same entitlement to be submitted, settled, or granted repeatedly.

Effective protection usually requires all of:

- Authoritative asset state
- Atomic commit
- Idempotent protection
- Audit ledger

If any of these four is missing, a duping hole will sooner or later surface from some edge chain.

## 1. How Item Duping Usually Arises

Common root causes include:

- Trading commit and rollback timing disorder
- State desync between inventory, mail, and auction
- Retries causing duplicate grants
- Double commits within the disconnect-reconnect window
- Duplicate execution of GM or compensation scripts

So item duping is rarely a single-point bug; more often the state boundary or commit semantics went wrong.

## 2. The Core Principle Is "One Authoritative Commit Point per Asset Change"

For example:

- Inventory item removal is authoritatively committed by the inventory system
- Mail claiming is uniformly committed by the mail and asset grant chains
- Final trade exchanges are uniformly committed by the trading system

Multiple modules must not each be able to modify the same final asset result on their own.

## 3. Idempotence Is the Baseline Capability Against Item Duping

As long as there exist:

- Client retries
- Network timeouts
- Service replays
- Task compensation

Then the same item grant or transfer must carry a unique operation ID.

A more sound semantic should be:

- Whether this operation has already been executed
- If already executed, return the existing result

Rather than "run it once more and see if it duplicates".

## 4. Trading, Mail, Auctions, and Quest Rewards Are High-Risk Areas

These chains are most prone to duplication problems because they all involve:

- State transitions
- Asset transfers
- Client confirmation
- Asynchronous notification

So these systems should not rely only on "client button order", but must have clear atomic commit and failure recovery rules.

## 5. Logs and Ledgers Are Not Add-ons; They Are Hemostatic Tools

Without a ledger, after a problem occurs you can usually only see:

- The player gained extra items

But cannot see:

- Which chain it came from
- How many times it duplicated
- Which request succeeded first

So high-value item changes should retain as much as possible:

- Operation ID
- Source system
- Quantity change
- Before and after state

## 6. A More Sound Approach in Engineering

A common combination is:

- Single authoritative asset system
- Atomic commit on critical chains
- All grants and transfers carry idempotent IDs
- High-value operations retain ledgers and reconciliation

This way, even if a bug occurs, it is easier to stop the bleeding and compensate quickly.

## 7. Common Misconceptions

### 7.1 If the Trading System Is Fine, Item Duping Won't Happen

Wrong. Mail, quests, compensation, auctions, and reconnection can all be entry points.

### 7.2 Database Transactions Solve Everything

Transactions only protect the boundary they live in; retries, callbacks, and asynchronous chains can still execute repeatedly.

### 7.3 Item Duping Is Always a Cheat

Many real incidents are actually due to idempotence and state machine holes in the business chains.

## References

- Online game asset consistency, idempotent granting, and anti-duplication practice materials
