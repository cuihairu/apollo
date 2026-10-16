[English](q44-inventory-system.en.md) | [中文](q44-inventory-system.md)

# Q44: How to Design an Inventory System?

## Core Conclusion

An inventory system is superficially slot management, but is essentially a combined system of "item ownership plus capacity constraints plus asset security".

The real difficulty is usually not dragging items, but:

- Item uniqueness and stacking rules
- Binding, expiration, and untradable restrictions
- Concurrent operations and duplicate deduction/issuance
- Edge cases of full inventory, splitting, sorting, and batch operations

As long as you treat the inventory as a simple array, problems will appear later once trading, mail, auction, and loot are connected in.

## I. First Clarify the Responsibilities of the Inventory System

It usually needs to be responsible for:

- Storing the player's held items
- Managing slots or capacity
- Handling add, remove, move, split, merge
- Supporting peripheral systems like use, sell, trade, mail attachments, auction

This means the inventory is both a presentation system and the entry point of the asset system.

## II. The Item Model Must Be Distinguished First

At minimum you usually have to distinguish:

- Stackable items
- Unique instance items
- Equipment-type instances
- Temporary items
- Bound items

If all items use the same structure, the rules get harder and harder to write later.

## III. The Difference Between Slot-based and Capacity-based Inventories

### 1. Slot-based

Emphasizes more:

- Slot limits
- Sorting and placement
- Splitting and merging

This kind of system is closer to the traditional RPG inventory experience.

### 2. Capacity-based

Emphasizes more:

- Total item count or total weight
- Does not strongly depend on specific slot positions

This suits weak-presentation, function-oriented scenarios.

Different designs directly affect the client presentation and the server-side data model.

## IV. Stacking Rules Must Be Very Clear

You have to define first:

- Which items can stack
- The maximum stack count
- Whether items with different binding states can merge
- Whether items with different expiration times can merge

These all directly affect the later add and split algorithms.

## V. Why Inventory Operations Must Emphasize Atomicity

The inventory is not ordinary UI state; it is often bound to high-value assets.

For example:

- Pickup
- Consumption
- Trade deduction
- Mail claim
- Auction listing

If these operations are not handled atomically, it is very easy to get:

- Duplicate acquisition
- Duplicate deduction
- Display inconsistent with real state

## VI. The Boundary Between the Inventory System and External Systems Must Be Stable

A more prudent principle is usually:

- External systems submit "add/remove/modify item requests"
- The inventory system uniformly does validity validation and submission

Rather than each subsystem directly mutating inventory data.

This makes it easier to guarantee:

- Consistent capacity checks
- Consistent concurrency rules
- Traceable audit and transaction logs

## VII. Points Requiring Special Attention in Engineering

### 1. Unique Instance Items

Equipment, items with random affixes or enhancement levels, should usually have an independent instance ID.

### 2. Concurrency and Duplicate Requests

Repeated client clicks, resends after disconnects, and cross-system retries can all mess up the inventory chain, so key operations need idempotency and locking strategies.

### 3. Change Logs

High-value item changes are best kept with:

- Source
- Destination
- Quantity
- Associated operation ID

## VIII. Common Misconceptions

### 1. The Inventory Is Just a `vector` or Array

The presentation layer may look like an array, but the system layer is far more than that.

### 2. As Long as the Client Can Drag to Sort, the Server Just Follows

The client can only express intent; final legitimacy should still be decided by the server.

### 3. All Items Can Go Through Unified Stacking Logic

Unique instances, binding state, and expiration times all break this simplification.

## References

- Various online-game materials on inventory, item instantiation, and asset transaction practices
