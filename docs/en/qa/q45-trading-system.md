
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q45: How to Design a Trading System?

## Core Conclusion

A trading system is not "both sides drop in something and click confirm"; it is a high-risk asset exchange chain.

What truly needs to be solved first is:

- Whether both trading parties' states are stable
- Whether the traded items and currency are locked
- How to roll back or cancel if any step is interrupted
- Whether the final commit is atomic

If these boundaries are unclear, duping holes, disconnect holes, and script retry problems will all erupt at once.

## 1. First Clarify the Trading Type

Common trading has at least two categories:

- Face-to-face real-time trading
- Asynchronous market or auction trading

Although both are called trading, the system structures differ greatly.

`q45` is better suited to focusing on direct trading between players.

## 2. The Core Flow of Face-to-Face Trading

A more sound trading chain usually includes:

1. Initiating the request
2. The other party accepts
3. Both sides enter the trading state
4. Placing items or currency
5. Locking the content
6. Both sides confirm
7. Atomic commit or unified cancellation

It looks simple, but every step needs state constraints.

## 3. Why "Locking" Is Critical

If items and currency in a trade are not locked first, typical problems appear:

- Being used again after being placed in the trade slot
- Being mailed or sold again after being placed in the trade slot
- Confirming on one side while continuing to change the content on the other

So the trading system usually needs to put participating assets into a restricted state before commit.

## 4. The Final Commit Must Be an Atomic Exchange

Once a trade enters final confirmation, the result should be only one of two:

- Both sides fully succeed
- Both sides remain unchanged

These must not occur:

- A's items deducted but B received nothing
- A received money but B received no goods

This is the most basic correctness requirement of a trading system.

## 5. Cancellation and Abnormal Interruption Must Be Designed Too

In real production environments, exceptions are more common than normal commits.

For example:

- One side disconnects
- One side times out without confirming
- One side's inventory space changes
- Service restart or node switch

All these scenarios need clear rules:

- Automatic cancellation
- Unlocking assets
- Restoring temporary state

## 6. The Trading System Must Tightly Coordinate with Inventory and Currency Systems

Trading is not an isolated system; it depends on at least:

- Inventory locking and capacity checks
- Currency deduction and addition
- Bound item validation
- Rules such as blacklists, level, and map restrictions

So the trading system is more like "a trading state machine plus an asset commit coordinator".

## 7. Anti-Fraud and Anti-Duping Are the Top Priority

Common risks include:

- Repeated clicking confirm
- Duplicate commit after disconnect and reconnect
- Tampering with quantities during a trade
- Exploiting the rollback window to duplicate assets

A more sound approach is usually:

- The server authoritatively maintains trading state
- The final commit carries idempotent protection
- The trading process has complete audit logs

## 8. A More Sound Design in Engineering

Common practices are:

- Establish a trading session object
- Both sides' assets enter locked state
- Clear the confirmation state after changing content
- The final commit uses a single atomic exchange
- Write a ledger for both success and failure

This makes it easier to prevent state pass-through and duplicate commits.

## 9. Common Misconceptions

### 9.1 Both Sides Clicking Confirm Is Enough

Not enough. After confirmation, atomic commit and exception recovery are still needed.

### 9.2 The Client's Trade Slot State Is the Final Truth

Wrong. The client can only display; session state must be authoritatively maintained by the server.

### 9.3 The Trading System Only Needs to Prevent Cheats

Many incidents come from disconnects, retries, timing, and internal bugs, not just cheats.

## References

- Various online game P2P trading, asset locking, and anti-duping practice materials
