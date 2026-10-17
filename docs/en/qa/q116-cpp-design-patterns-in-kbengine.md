# C++ Design Patterns in KBEngine

This is an external-engine learning reference. Pattern names describe ways to organize responsibilities; they do not define Apollo's process topology or certify that a particular feature is implemented. See [Apollo architecture status](../analysis/architecture-review.md).

## Patterns and responsibilities

| Pattern | Example responsibility | Benefit | Tradeoff |
|---|---|---|---|
| Singleton | Process-wide application access | A common access point | Hidden dependencies and harder isolation in tests |
| Factory | Entity construction by registered type | Decouples callers from concrete entity types | Registration and lifetime errors must be visible |
| Observer | Change notification and monitoring | Separates producers from consumers | Unsubscription, callback lifetime, and reentrancy require care |
| Proxy | A local reference representing a remote entity | Hides location-dependent invocation mechanics | Transport success does not establish business completion |
| Command | Message handlers and queued intentions | Supports controlled dispatch and execution order | Requires validation, correlation, and bounded queues |
| Object pool | Reuse of frequently allocated objects | Can reduce allocation overhead | Reset correctness and retention limits matter |
| Bridge | Network interface and transport implementation | Keeps abstraction and implementation independent | Adds interfaces that need clear ownership |
| Template method | Shared lifecycle with application hooks | Keeps startup and execution order consistent | Inheritance can create implicit ordering constraints |
| Strategy | Placement and load-balancing decisions | Allows policies to vary behind one interface | Policies still need shared admission invariants |
| Chain of responsibility | Staged message validation and routing | Keeps processing stages focused | Dropped or multiply processed requests need explicit outcomes |
| Composite | Hierarchical runtime ownership | Represents collections and contained units uniformly | Ownership does not imply distributed authority |

## How to evaluate a pattern

A useful review starts with ownership and failure behavior rather than a pattern label. Identify who constructs and destroys an object, which thread may mutate it, whether callbacks can outlive their target, and how a failure reaches the caller.

For network-facing code, separate decoding, authorization, routing, and simulation execution. A queued command needs a request identity and an overload result. A remote proxy needs timeout and retry semantics. An observer must not let a slow consumer block the authoritative simulation loop.

For persistent state, distinguish accepted work from durable work. A callback indicating queue insertion is not proof that a reward survived a restart. Retries require business idempotency even when transport correlation works correctly.

## Applying the lessons to Apollo

Apollo's reusable host and module lifecycle provide the common runtime boundary. World hosts Scene/Instance simulation and player data services; Login hosts authentication and directory/placement control; Gateway holds client connections. These responsibilities remain stable regardless of whether their implementation uses factories, observers, strategies, or composition.

Ghost/proxy concepts from KBEngine's distributed spatial model are not a requirement for Apollo's explicit cross-World transfer. Likewise, a manager strategy does not justify adding another business process when the same responsibility can be hosted by an existing application.

Prefer a small interface whose ownership and outcomes can be tested. Introduce pooling after measurement, keep policy separate from mechanism, and avoid process-wide mutable access where dependency injection makes the dependency explicit.

For the extended worked examples, see the [Chinese design-pattern reference](/qa/q116-cpp-design-patterns-in-kbengine). For the relationship between source areas and process responsibilities, see [KBEngine source analysis](../architecture/kbe-source-analysis.md).
