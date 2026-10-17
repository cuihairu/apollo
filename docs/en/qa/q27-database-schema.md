
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q27: How to Design Database Table Structures?

## Core Conclusion

The core of MMO table structure design is not flattening all objects into a bunch of tables, but first clarifying:

- Which data is the authoritative master record
- Which data changes at high frequency
- Which data suits table splitting or event-ization
- Which queries really need to be supported

A good table structure is driven by business access patterns, not mechanically split by "system module name."

## I. First Split by Data Nature, Not by Code Class Name

Common layers can be divided into:

### 1. Account Layer

For example:

- Account
- Login information
- Ban status
- Device and risk-control information

### 2. Character Master Record Layer

For example:

- Character basic information
- Class, level, experience
- Home server
- Current basic growth state

### 3. Subsystem Layer

For example:

- Inventory
- Quests
- Mail
- Friends
- Guild
- Auction house

These modules have different change frequencies, capacity limits, and query patterns; they usually do not suit being forced into one big table.

### 4. Journal and Log Layer

For example:

- Currency change journals
- Item change journals
- Trade records
- Reward grant records

This kind of data is for audit and traceability; it should not be mixed into the master record tables.

## II. The Character Master Record Table Should Be as Stable as Possible

The character master record table is suitable for:

- Primary key and foreign key relationships
- A small number of high-value core fields
- Character basic information that needs fast reads

It is not suitable for unlimited stuffing of:

- Long lists
- Large JSON objects
- High-frequency changing fine-grained sub-states

Otherwise the master record table becomes a super-wide table, and later expansion and writes will both be painful.

## III. When to Split Tables

When the following situations appear, table splitting should usually be considered:

- Field count keeps bloating
- Query patterns are completely different
- Update frequencies differ greatly
- Data lifecycles differ
- Storage scale grows significantly

For example:

- Player basic information and inventory data should not be in the same table
- Mail body and mail index are often separated too
- Leaderboard snapshots and the real-time character master record do not suit sharing a table

## IV. Relationship Modeling Must Serve Real Queries

When designing table structures, you must first ask:

"How will this table be queried in the future?"

For example, in a friend system, the really common queries are:

- Look up a player's friend list
- Check whether two players are already friends
- Look up online friends

Then indexes and relationship tables should be designed around these queries, rather than purely pursuing normal-form elegance.

## V. Do Not Overly Rely on a Single Large JSON Field

Stuffing complex modules into JSON does save development time, but the long-term cost is high:

- Hard to query
- Inconvenient for partial updates
- Poor indexing capability
- Troublesome data validation and migration

JSON is suitable for:

- Extension fields
- Low-frequency access configuration
- Small structures that do not need complex queries

But it is not suitable as the main carrier of the core master record.

## VI. Index Design Matters More Than "Whether Fields Are Complete"

Many online game problems are not too few fields, but indexes pointing in the wrong direction.

Common principles:

- Primary keys simple and stable
- High-frequency queries must have direct indexes
- Avoid meaningless excessive indexes
- Composite indexes should closely match real where conditions

Too few indexes are slow; too many indexes amplify write cost.

## VII. High-Value Systems Need Journal Tables

For systems like currency, items, auctions, and mail rewards, you cannot keep only the "current state."

A more robust approach is usually:

- The master record table keeps the current result
- The journal table keeps the change process
- Idempotency IDs run through key operations

Only this can support:

- Reconciliation
- Rollback compensation
- Cheat investigation

## VIII. Don't Shard Too Early, but Don't Pretend You'll Never Need It

Not every project needs sharding from day one, but the table structure should at least avoid obviously blocking future splitting.

For example:

- The primary key strategy must be extensible
- Minimize cross-module strong transaction binding as much as possible
- Reserve the possibility of splitting by server, account, or character dimensions

## IX. Common Misconceptions

### 1. Fewer tables means simpler

Possibly simple short-term, but long-term it usually becomes a coupling disaster.

### 2. Normalize everything first, deal with performance later

Many online game tables are naturally read-write mixed; designing strictly by textbook normal forms may not be appropriate.

### 3. All subsystems use one generic "big field extension"

This usually sacrifices query capability and evolution capability.

## References

- Various online game character master record, journal table, and database/table sharding practice materials
