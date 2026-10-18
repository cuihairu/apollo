# Sessions and Online Routing

Resolve profile ownership, simulation Space/Cell/executor, and gateway connection separately. See the [BigWorld–Apollo mapping](./bigworld-apollo-mapping.md).

## Authority and location projections

loginServer hosts service and player directories. worldServer exclusively owns topology and placement for its Spaces/Cells. Executors register with worldServer; machined never assigns business ownership. Directories store committed location projections and gateway caches forwarding routes; neither becomes another topology authority. Business traffic goes directly to Base or cell.

## Records

| Mapping | Purpose |
|---|---|
| Base identity → endpoint, readiness, process generation | Persistent business and independent Base replacement. |
| World identity → management endpoint, readiness, capacity, versions | Select a normal or cross-server management domain. |
| Executor ID → World, endpoint, generation, readiness, load | worldServer manages registration/drain; one executor hosts several Cells. |
| Space/Cell → bounds, neighbors, executor, topology version | Authoritative World record; initially one Cell per Space. |
| Player → home server, Base, ownership version | Stable profile affiliation across cross-server play. |
| Player → World, Space, Cell, executor, optional Instance, simulation generation | Projection of committed simulation placement. |
| Player → session/gateway, session generation, route version | Connections, duplicate login, and reconnection. |
| Transfer ID → source/target, expected generations, prepared/committed result | Recover ambiguous handoffs; cross-World moves also require one commit decision. |

Keep Space, Cell, and executor IDs distinct, as well as process/authority generations, topology versions, and session versions. WorldAssignment does not yet contain all these fields. Existing home_zone_id is not a complete home-server identity; IP addresses and database credentials are not player ownership.

## Simulation handoff

Base validates admission and supplies snapshots; World selects an executor. Freeze the source and prepare the inactive destination. The management authority conditionally commits ownership, the target activates using commit evidence, directory projections update, gateway refreshes, and the source cleans up. Delayed projections must refresh, redirect, or reject stale generations, never restore source authority.

Cancel only after confirming no commit. Reconcile ambiguous outcomes from durable records; never reactivate both ends. Original Base/database still own profiles and settlement.

## Base and gateway replacement

Base independently transfers persistent objects, in-flight business, and write generations without changing storage affiliation. Fence old writes at business/storage boundaries; cell/gateway refresh Base caches.

Backend routing does not move sockets. Keeping gateway can retain TCP; replacing it requires drain, reconnect recovery, or explicit socket handoff. Reconnection restores original Base and current simulation placement.

## Implementation status

OnlineDirectory, SessionLocator, PlayerDirectory, and WorldAssignment are reusable primitives. Manager/executor separation, Base ownership, independent generations, distributed projections, and durable commits remain unfinished. Whole-profile migration tests do not validate this model.
