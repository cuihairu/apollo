[English](persistence.en.md) | [中文](persistence.md)

# Apollo Persistence Review (persistence)

> Belongs to: companion piece 6 of the phase-one read-only architecture review. Companions: audit.md (the A7 parallel-stack master table), architecture.md, object-model.md, lifecycle.md, improvement-plan.md.
> Basis: task book §7 (Memory Authority: online player memory is authoritative, the Gameplay→DB→Result pattern is forbidden), §19 (Repository/Storage/Cache/Snapshot/Journal/Write-Behind; Save timing/Shutdown Save/Crash Recovery/Player Logout/Instance Completion/Critical Transaction), §20 (Recovery: the three tiers must-persist / recomputable / droppable). Terminology follows existing code wording (BigWorld/KBE concepts reused first); the differences list is at the end.

---

## 1. Overall Verdict

**The code delivers in-memory, the design promises write-behind, and the real DB/Redis survives as dead code — a three-layer misalignment**. The repository contains **not a single persistence path that can reach a real MySQL/Redis**, nor any csv/sqlite flush chain (sqlite is only an ipc side branch with IPC=OFF, and vcpkg has no sqlite3).

Measured baseline (the build baseline audited by this batch, all 88 translation units with no `-D` macros): `APOLLO_USE_MYSQL_CONNECTOR`, `APOLLO_USE_REDIS_PLUS_PLUS`, `APOLLO_HAS_HIREDIS`, `HAVE_MYSQL` are all undefined; vcpkg.json dependencies are only openssl/protobuf/gtest/nlohmann-json/pugixml/readline.

---

## 2. Parallel-Stack Panorama (audit.md A7 expanded — 5 DB abstractions + 4 Redis implementations)

### 2.1 Database side: 4 namespace abstractions

| # | Namespace | Implementation files | State |
|---|---|---|---|
| B1 | apollo::storage::database (IDbConnection/DbFactory/DbManager/MySQLConnection/MockConnection) | src/apollo/storage/database/{db,db_common,db_mysql,db_mock}.cpp | Every file walled off by `#ifdef APOLLO_USE_MYSQL_CONNECTOR` or the factory else-branch returns nullptr; not in the modular build |
| B2 | apollo::database (IDatabaseConnection/DatabaseRegistry/SqlTemplate/DataSource/ConnectionPool) | src/apollo/database/{sql_template,datasource}.cpp + a duplicate copy of the same files under modules/data/orm/src | Legacy tree hangs off the legacy library only; orm/CMakeLists compiles only memory_connection+sql_template (the new headers have no implementation); datasource.cpp's createConnection is permanently empty since no factory registers |
| B3 | apollo::db (IDBConnection/ConnectionPool/MySQLConnection) | src/storage/database/connection_pool.cpp ≡ modules/data/orm/src/connection_pool.cpp (the same file twice) | ConnectionPool skeleton complete (lock+cv+deque+Stats) but **CreateConnection() is an empty function** (:139-147, comments only); neither copy is compiled into the modular build |
| B4 | apollo::data::{core,orm} (IConnection/IConnectSource/SimpleDataSource/MemoryConnection/SqlTemplate) | modules/data/orm/src/memory_connection.cpp + sql_template.cpp | memory_connection runs (exact SQL string matching, a miss silently yields empty results); **the new SqlTemplate's declared query/update has no definition in the repo** (nm confirms only apollo::database::SqlTemplate symbols) → any data-family link through SqlTemplate necessarily fails |

### 2.2 Redis side: 4 implementations (2 namespaces) + a 5th at the application layer

| # | Implementation | Files | State |
|---|---|---|---|
| C1 | RedisPlusPlusClient (redis++, all commands) | src/apollo/storage/redis/redis_client_impl.cpp + two copies under modules/data/redis/src (forked, diff 141 lines) | #ifdef walled off file-wide; modules/data/redis/CMakeLists.txt:24 hardcodes a Windows vcpkg path, :31-38 searches Windows only — never hits on Linux |
| C2 | apollo::net::redis::RedisTemplate (hiredis) | modules/data/redis/src/redis_template.cpp | Compiled into the library but connect() is permanently false ("hiredis not available"); the new header's `using RedisTemplate = apollo::RedisTemplate` is a dead alias (the class is in apollo::net::redis, the alias points at the wrong namespace) |
| C3 | apollo::db::RedisClient (hand-rolled RESP protocol, 2066 lines) | modules/data/cache/src/redis_cache.cpp + src/storage/cache/redis.cpp, two copies | Most complete command implementation (AUTH/GET/SET/EXPIRE/hashes…) but **entirely uncompiled** (cache/CMakeLists compiles only cache_manager+primitive_cache) |
| C4 | MockRedisClient | include/apollo/storage/redis/redis_mock.h + the factory default branch of src/apollo/storage/redis/redis.cpp | In-memory unordered_map; the only compiled redis implementation (legacy library); sole consumer test_redis_new.cpp (the GTest branch is not built by default) |
| 5th set | base-app DatabaseService | apps/base-app/src/database_service.cpp | Disconnected from all of the above (includes no data/storage headers); an instance-level in-memory stack |

**Duplicate-piece tally**: ConnectionPool ×2, same file; RedisFactory/RedisManager ×2 byte-identical (diff=0, the new directory holds legacy files); RedisClient ×2; redis_client_impl ×2 already forked; redis_client_wrapper.cpp/redis_template_wrapper.cpp are empty files. **9+ duplicated implementation files, zero ability to actually reach a medium.**

---

## 3. State of the Memory-Authority Model (task book §7 check)

### 3.1 Player objects (Anchor ↔ DB)

```text
Current state:
  load: DatabaseService::loadPlayer == cache_[id]; on a miss it constructs a bootstrap
        player username="player_+id" and writes it back to the cache (database_service.cpp:31-49)
        —— "a profile grown out of thin air", and toJson/fromJson are asymmetric
        (fromJson misses x/y/z, base_server.cpp:87-94)
  save: savePlayer is only cache_[id]=data (:48-52); savePlayerAsync completes synchronously;
        SaveQueue::workerLoop only does callback(true) and writes no medium (:158)
  autoSave: autoSaveLoop only prints "Auto-save triggered"; handleDbQueryRequest "Not implemented"
  conclusion: stop the process and all player data is lost
```

### 3.2 Authority-model comparison

| Task book §7 recommendation | Code | Verdict |
|---|---|---|
| Online Player memory is authoritative | **Yes** (the in-memory map is the single truth) | Half-met — authority in memory ✓, but the downstream (journal/snapshot/flush) is all empty ✗ |
| Gameplay→DB→Result anti-pattern forbidden | The current state is not the anti-pattern, it is "Gameplay→memory→(nothing)**" — **what is missing is not the authority direction but the flush** | Structurally half-done |
| Dirty/Journal/Snapshot three states | The dirty flag exists on PlayerAnchor (dirty_reasons_), but needs_save is consumed by tests only; journal zero implementation | ✗ |

### 3.3 Storage-medium inventory

| Medium | Code | Runnability |
|---|---|---|
| MySQL | B1 #ifdef walled off | ✗ |
| Redis | C1-C3 walled off/uncompiled/connect false | ✗ |
| SQLite | src/apollo/ipc/sqlite_service_discovery.cpp (IPC=OFF + no sqlite3 in vcpkg) | ✗ |
| File/JSON flush | none | ✗ |
| Memory | MemoryConnection + DatabaseService map | ✓ the only live path |

---

## 4. Save Timing and Operational Correctness Review (task book §19 checklist, item by item)

| Timing | Current state | Verdict |
|---|---|---|
| Normal save (dirty-driven) | none (autoSave idles; mark_dirty only sets the flag) | ✗ |
| Shutdown Save | none (no flush protocol at shutdown) | ✗ |
| Crash Recovery | journal/WAL/redo: zero hits repo-wide (grep-verified) | ✗ |
| Player Logout | no logout message type; deactivate erases directly | ✗ |
| Instance Completion | no instance lifecycle (settlement results have nowhere to land) | ✗ |
| Critical Transaction | transactions exist only in mocks (test_database.cpp) and the legacy SqlTemplate implementation; no production consumer | ✗ |
| Data consistency | no version/delta comparison; the fromJson/toJson asymmetry is a live round-trip defect | ✗ |

**The only "real" save path**: the game-server demo's MemoryConnection seed_query (exact SQL string match, a miss returns an empty row set without error) — demonstrative, and any link through the SqlTemplate layer fails to link (§2.1 B4).

---

## 5. Target Model (contract §1.5 + attribute-sync §8.2; improvement-plan P1 anchor)

```text
Load:   startup/login -> read DB (or warmed cache) -> Anchor in-memory authority
Runtime:Anchor online state in-memory authority; L3 volatile state (HP/position) not archived
Dirty:  write-behind journal (PersistJournal): changes append to the log first (doubling as
        the hot-standby stream / recovery position)
        a fixed quota drained evenly per tick (attribute-sync §8.2: fixed ratio per tick, bounded queue)
Snapshot:periodic snapshots thin the journal; process restart replays from the journal position
Redis:  cross-process hot data only (online directory mirroring / subscription push),
        **not an entity cache** (attribute-sync's line)
Layers: Repository (DAO mapping) -> Storage (medium abstraction) -> Cache (optional read cache) -> Journal
```

Mapped against task book §19: Repository/Storage/Cache/Snapshot/Journal/Write-Behind, the six pieces, **none exist** in the current code; with all six of the target model in place, the seven timings of §4 become answerable one by one.

---

## 6. Terminology Differences (persistence level)

1. **`PersistJournal` (write-behind journal)**: the original line, §1.5, log first then snapshot flush; zero code implementation (grep for journal/snapshot/redo/undo/WAL gets zero hits in the data family; the only hits are log_manager's "log snapshot" vocabulary) — **the naming of the original line exists, the implementation is missing**. Difference registered.
2. **Original line "Redis only as cross-process hot data"** (attribute-sync's line): the code-side Redis chain is fully walled off, neither deviation nor implementation — registered as "no code to align against".
3. **`AnchorManager` vs. archives**: the original line says the Anchor stays resident on disconnect to keep state, and archiving goes through write-behind; the code's save is a fake implementation — functional gap registered (persistence §3.1).
4. **MemoryConnection's "silent empty result"** (an unmatched SQL returns ok=true): contradicts task book §25 error handling (Recoverable/Retryable/Fatal made explicit); a bug-face item, registered in improvement-plan P1-4.
5. For the rest (the Repository/DAO surface) the original book set no terms; code words or industrial words are used (e.g. `Repository`), conflicting with no one.

---

(end)
