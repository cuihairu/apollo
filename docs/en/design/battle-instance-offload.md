# Instance Execution and Optional Process Isolation

The default model creates an Instance for a group inside World. Reuse World, Scene, Instance, and BattleRuntime; release resources and destroy the instance after gameplay and settlement. See [World and Instance design](../30-Instance_GameServer_Design.md).

If gameplay later requires stronger fault or resource isolation, a World process may run only one Instance. This is a deployment choice, not another target process name. See ADR-012 in the [current architecture decisions](../architecture/adr.md).

Existing offload modules and in-memory transport stubs do not provide production auto-spawn. Spawn requests, readiness, admission, placement publication, process exit, timeouts, and crash failure require integration and validation. Settlement retains the existing persistence authority; isolation must not introduce another writer.

Auto-scaling and one process per instance are optional. First validate multiple isolated instances in one World, group-triggered creation, objective completion, and destruction.
