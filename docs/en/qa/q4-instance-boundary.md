# Q4: How are gameplay instances created, completed, and destroyed?

World hosts game logic. Instance is a temporary activity created for a group of players. Scene owns entities, AOI, and ticks inside World; Instance owns the activity lifecycle.

After a group request, validate members, select World, and create an Instance and temporary Scene. Players enter, clear enemies and objectives, and receive idempotent settlement. After they leave, release entities, tasks, scripts, and scene resources, then destroy the instance. Empty groups, departures, failures, and timeouts need explicit cleanup.

World, Scene, and the Instance state machine exist. Group admission, objective settlement, and automatic destruction are not integrated end to end. Cross-World transfers separately require authority handoff and Gateway route validation. See [World and Instance design](../30-Instance_GameServer_Design.md) and the root [`todo.md`](https://github.com/cuihairu/apollo/blob/main/todo.md).
