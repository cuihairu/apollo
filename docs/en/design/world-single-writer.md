# Simulation Threads and Write Ownership

The target executor is cell; worldServer manages Space/Cell topology. Retain this document path while correcting responsibilities. Current WorldApp still combines both roles; see the [mapping](./bigworld-apollo-mapping.md).

## Target rules

An entity has one authoritative Cell writer at a time. Its logical executor sequentially modifies local entities, AOI, and simulation. IO, background, and debug work submit bounded intents. One executor may host Cells from multiple Spaces; neither Space nor Instance is a process.

Initially each Space has one Cell. Future multi-Cell Spaces execute partitions independently and send cross-partition actions to the entity authority for adjudication. Shared Instance objectives, clocks, and settlement need one logical owner, without shared memory or global lockstep. Base retains profile writes; database validates and persists them separately.

## Current code

WorldApp::gameLoop pumps M1, accepts intents into acceptanceQueue_, consumes them at fixed boundaries, runs Lua, and calls WorldHost::tick. Closing admission precedes a final queue sweep and host teardown. Move this simulation loop into cell while retaining management in worldServer; existing class names do not prove the target deployment exists.

## Overload and shutdown

Reject overload explicitly; never discard accepted intents silently. Close admission, sweep work, then stop simulation and links. Define drain deadlines, settlement policy, failure replies, and forced-stop recovery. Distinguish admission, execution, and persistence.

## Concurrency acceptance

- IO/background workers never mutate simulation objects directly; each entity has one effective authority generation.
- Input order is traceable and duplicate requests do not duplicate business writes.
- Queue saturation, shutdown races, execution failure, and forced stop have explicit outcomes.
- Targets remain inactive before commit; fence sources afterward, including shared Instance ownership.
