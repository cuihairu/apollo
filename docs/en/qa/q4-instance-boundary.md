# Q4: How are instances created, completed, and destroyed?

Base validates the group, worldServer places Space/Cell/executor, and cell creates/runs the temporary Instance. Space matches existing Scene semantics; Instance owns members, objectives, clocks, settlement, and cleanup, without another spatial layer or process.

Send completion/failure results to owning Bases for validation and durable database confirmation, then depart and release temporary resources. Persistent Base objects survive. Define cancellation, no entry, disconnect, and timeout outcomes.

World/Scene/Instance primitives exist; manager/executor separation and complete distributed wiring remain unfinished. Initially one Cell per Space; reserve future multiple Cells/seamless maps. See the [mapping](../design/bigworld-apollo-mapping.md), [instance design](../30-Instance_GameServer_Design.md), and root todo.md.
