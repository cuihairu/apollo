[English](q46-auction-house.en.md) | [中文](q46-auction-house.md)

# Q46: How do you design an auction house?

## Core Conclusion

An auction house is not "an offline version of the trading system"; it is "an asynchronous market plus a search index plus a settlement and clearing system".

The real difficulty is usually not the listing screen, but:

- Escrow and locking of listed items
- Search indexing and sorting
- Deal settlement and fees
- Expiration, cancellation, and bidding conflicts

Once the player base grows, the auction house quickly becomes a comprehensive system spanning data, caching, search, and asset safety.

## One: First Distinguish the Auction House Business Models

There are two common kinds:

- Fixed-price listing
- Bid-based auction

The two differ greatly at the system level:

- Fixed-price is closer to a commodity market
- The bidding model emphasizes deadlines, bid ordering, and final settlement

Many projects run both, but they should not be mixed into one ambiguous flow.

## Two: The Fundamental Difference Between an Auction House and Face-to-Face Trading

An auction house is asynchronous trading, and usually has these characteristics:

- Both sides do not need to be online simultaneously
- Traded items are held in escrow by the system
- The deal happens at an independent point in time
- Query and filtering pressure is heavy

So it is more like a "market service" than simple two-person interaction.

## Three: The Most Critical Part of Listing Is Escrow

Once an item enters the auction house, it must leave the player's free inventory.

That is, the system must either:

- Directly transfer item ownership into an auction escrow state
- Or at least lock the item and forbid all other operations

Otherwise typical problems appear:

- The item can still be used or traded after being listed
- Conflicts between delisting and deals
- Duplicate listing

## Four: Search and Filtering Are the Performance Core of an Auction House

Players usually search by these dimensions:

- Type
- Quality
- Profession
- Price range
- Remaining time

So the auction house system cannot only think about "having a listing table"; it must also consider:

- Search indexes
- Sort fields
- Pagination performance
- Caching of popular filter conditions

This is also why auction houses often need an additional cache or search layer.

## Five: Deals and Clearing Must Be Designed Independently

After a deal, the system usually still has to handle:

- Deduction
- Ownership change of the item
- Fee
- Crediting the seller's proceeds
- Notifying buyer and seller

This is a complete clearing chain; it cannot be treated as just "updating one status".

## Six: The Bidding Model Especially Needs Attention to Timing

If bidding is supported, you must first define:

- Minimum bid increment
- Deadline
- Rules for bids at the very last moment
- How funds are handled after being outbid

Otherwise concurrent bids near the end easily cause problems.

## Seven: A More Solid Organizational Approach in Engineering

A common approach is:

- The main listing table holds the trade's main state
- A search index layer carries queries and sorting
- The asset system handles escrow and deal clearing
- Scheduled tasks handle expiration, unsold items, and settlement

That way the auction house does not cram search, assets, and notifications into a single table.

## Eight: Risk Control and Auditing Are Very Important

An auction house is inherently a high-risk system, because it is easily used for:

- Money laundering
- Studios transferring assets
- Exploiting abnormal pricing for arbitrage

So you usually also need:

- Listing and deal ledgers
- Abnormal price monitoring
- Account correlation analysis
- Restrictions on high-risk items

## Nine: Common Misconceptions

### 1. An Auction House Is Just a More Complex Trade

Not accurate. It also includes search, escrow, settlement, and risk control.

### 2. Having a Database Table Is Enough for Search

Once the scale grows, sorting and filtering quickly become performance bottlenecks.

### 3. After a Deal, Just Swap the Money and Item Directly

In practice you still have to handle fees, expiration, notifications, retries, and idempotency.

## References

- Various online game auction house, market index, and asset clearing practice materials
