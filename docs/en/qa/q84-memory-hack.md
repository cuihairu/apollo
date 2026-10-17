
> This is general knowledge or an external-engine reference, not an Apollo architecture or implementation claim. See [Apollo's architecture status](../analysis/architecture-review.md) for target terminology and verified implementation boundaries.

# Q84: How do you prevent memory modification?

## Core Conclusion

Memory modification protection cannot be built on the premise that "client memory can always be held".

A more realistic principle is:

- Client memory may be read, modified, and injected into
- High-value results must not depend only on client memory
- Client-side protection can only raise the cost; it cannot replace server-side authority

So the core of preventing memory modification is not absolute prevention, but limiting the damage it can cause to a small range.

## One: What Memory Modification Can Actually Affect

Commonly includes:

- Locally displayed numbers
- Locally displayed cooldowns
- Local visibility information
- Input assistance logic

If the system design is improper, it may further affect:

- Illegal movement
- Skill spam
- Item and currency changes

But these high-value results should never be directly determined by client memory in the first place.

## Two: Server-Side Authority Remains the First Principle

For example:

- Health is settled by the server
- Currency and items are based on server state
- Skill cooldowns are based on server time

That way, even if the client changes values in local memory, it at most affects local presentation, and cannot directly rewrite the final result.

## Three: The Value of Client-Side Protection Lies in Raising the Bar

Common techniques include:

- Anti-debugging
- Anti-injection
- Integrity verification of key modules
- Memory layout perturbation and packing

These measures cannot guarantee absolute security, but they raise the cost of producing and maintaining cheat tools.

## Four: Don't Mix Up "Display Security" with "Logical Security"

Many local memory data, even if changed, only affects display.

What is truly dangerous is local state that affects the semantics of requests sent to the server, for example:

- Local aim assistance
- Automated determination
- Illegal visibility assistance

So protection priorities should be ranked by "whether it can affect the final result".

## Five: Behavioral Risk Control Must Be Layered On Top

Even if client-side protection is done well, you cannot rely on it alone.

A more solid system is usually:

- Client integrity protection
- Server-side authoritative validation
- Anomaly behavior statistics

These three layers working together can both raise the bar and avoid relying on any single layer.

## Six: A More Solid Approach in Engineering

Common practices are:

- High-value state defers only to the server
- The client adds basic integrity and anti-injection detection
- Suspicious behavior enters risk scoring
- Punishments depend on multi-dimensional evidence rather than single-point client detection

## Seven: Common Misconceptions

### 1. Preventing Memory Modification Is the Whole of Client Security

No. Many cheat tools can implement automation and information enhancement without modifying core memory at all.

### 2. Packing Alone Is Enough

Packing can only raise the cost of reverse engineering; it cannot establish business trustworthiness.

### 3. If the Client Value Is Changed, an Incident Will Definitely Happen

The premise is that the server treats it as real. The real root cause usually lies at the trust boundary.

## References

- Materials on client integrity, anti-injection, and server-side authority boundaries
