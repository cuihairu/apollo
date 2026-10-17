
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q63: How to Optimize Database Queries?

## Core Conclusion

The core of database query optimization is not memorizing how many SQL tricks you know, but first figuring out:

- Which queries are the most expensive
- Why they are expensive
- Whether the number of queries can be reduced

Many database problems are not "the SQL is not fancy enough", but the system putting all the pressure that should not land on the database onto the database.

## I. First Judge Whether the Problem Is the Query Itself

Common database slowness may come from:

- Poor SQL statements
- Wrong indexes
- Unreasonable table design
- Too many queries
- Hot data not cached

If you don't distinguish the cause first, you easily end up only adding indexes blindly.

## II. Reducing the Number of Queries Is Usually More Valuable Than Optimizing a Single SQL

Common high-benefit approaches include:

- Batch queries
- Caching hot data
- Reasonable preloading
- Avoiding N+1 queries

The real bottleneck of many systems is not one particular SQL being too slow, but the same kind of small query being hit too many times.

## III. Index Optimization Should Revolve Around Real Access Patterns

More indexes are not always better.

When designing indexes you should first ask:

- What are the most common where conditions of this query
- Is there sorting and pagination
- Is a covering index all that is needed

If it does not match the real access pattern, more indexes are useless and also amplify write cost.

## IV. Deep Pagination, Fuzzy Queries, and Large-range Scans Require Special Care

These kinds of queries most easily break once the business grows.

Common fixes include:

- Use cursor- or primary-key-based pagination instead of deep pagination
- Hand search to a more suitable index or search layer
- Isolate reports from the online primary database

Don't expect one "universal SQL" to handle both online queries and analytical queries at the same time.

## V. Read/write Splitting and Caching Are Not Silver Bullets

They are indeed common, but the prerequisites are:

- The business can tolerate read latency
- Cache invalidation strategy is clear
- They do not create new hotspots

Otherwise you just move the problem somewhere else to explode.

## VI. Query Optimization Must Be Looked at Together with Table Structure

If the table structure itself is unsuitable for queries, for example:

- Ultra-wide tables
- High-frequency and low-frequency fields mixed together
- Large amounts of JSON that are hard to filter

Then no matter how you change the SQL, you can only stop the bleeding.

## VII. A More Pragmatic Optimization Order in Engineering

Common practice is:

1. First capture slow queries and high-frequency queries
2. First reduce the number of queries
3. Then adjust indexes and SQL
4. Finally consider table splitting, read/write splitting, and a search layer

This is usually more effective than refactoring the database from the start.

## VIII. Common Misconceptions

### 1. If the Database Is Slow, Add an Index First

Not necessarily. The real problem may be the query pattern or the data model being wrong.

### 2. Changing `SELECT *` to Column Names Counts as Optimization

Sometimes useful, but often not the main bottleneck.

### 3. All Slow Queries Should Go Into the Cache

Caching can alleviate some problems, but it brings consistency and origin-fetch pressure; it depends on the scenario.

## References

- Materials on slow query analysis, index design, and online database governance practices
