
# Data Architecture Whitepaper

> Purpose: unify Apollo MMO's player data, operational BI, caching strategy, and TLog design; resolve the existing "Blob vs split-table" conflict in the documentation; and provide an authoritative reference for DataProxy and database design.

## 1. Design Principles
1. **Game performance first**: the logic server reads and writes via Protobuf/object serialization, minimizing DB round trips.
2. **Queryability**: operational and BI core fields must be columnar, supporting SQL queries/aggregation.
3. **Eventual consistency**: most data lands asynchronously; critical assets (top-up, trading) get synchronous guarantees.
4. **Hybrid storage**: primary DB (MySQL 8.0) + Redis cache + Kafka/ClickHouse TLog; Mongo/ES introduced for specific needs when necessary.
5. **Extensibility**: database/table sharding, horizontal scaling, online migration; a unified DataProxy layer shields the underlying changes.

## 2. Data Classification and Storage

| Data type | Characteristics | Storage strategy | Examples |
|----------|------|----------|------|
| Player master data | Strong consistency, complex structure | Hybrid Schema: core columns + Blob (Proto) | Level, combat power, attributes, inventory |
| Runtime state | High-frequency change, short lifecycle | Redis / memory, periodically flushed to DB | Online status, scene position, party |
| Economy/trading | Strong consistency, audit-sensitive | MySQL transactions + TLog dual write | Trading, auction, top-up records |
| Social data | Medium consistency, many relational queries | MySQL split tables + Redis cache | Guild, friends, mail |
| Behavior logs | Append-only, analysis-oriented | TLog -> Kafka -> ClickHouse | Gold flow, level logs |
| Configuration/static | Read-only / rarely updated | Configuration system + CDN/versioning | Excel → Lua/JSON |

## 3. Hybrid Schema in Detail

### 3.1 Table structure (example `t_character`)
```sql
CREATE TABLE `t_character` (
    `char_id` BIGINT NOT NULL,
    `account_id` BIGINT NOT NULL,
    `name` VARCHAR(64),
    `level` INT,
    `vip_level` INT,
    `power` BIGINT,
    `gold` BIGINT,
    `diamond` BIGINT,
    `map_id` INT,
    `last_login` DATETIME,
    `data_bin` MEDIUMBLOB,
    `version` BIGINT DEFAULT 0,
    PRIMARY KEY (`char_id`),
    KEY `idx_account` (`account_id`),
    KEY `idx_level_power` (`level`, `power`)
);
```

### 3.2 Blob content
- Serialized with Protobuf (`PlayerProfile`): contains the attribute map, inventory, quests, skills, and other sparse data.
- Blob updates use old-vs-new version comparison (optimistic lock on the `version` field) to avoid overwrites.
- DataProxy handles Protobuf serialization/deserialization; business logic operates only on in-memory objects.

### 3.3 Columnar fields
- Only high-frequency query/filter fields are made columnar (level, combat power, economy, map, timestamps, etc.).
- Update flow: in-memory object update → mark dirty fields → DataProxy synchronously updates the column values when writing the DB.

## 4. DataProxy Layer

### 4.1 Responsibilities
- Shield business code from database details, providing APIs such as `LoadPlayer`, `SavePlayer`, `QueryPlayers`, `UpdateEconomy`.
- Maintain Redis + MySQL consistency (Write-Through/Write-Behind strategies).
- Manage sharding routing (ShardingKey = char_id/account_id).
- Async write queue + WAL (local journal) to guarantee power-failure recovery.

### 4.2 Write strategy
| Scenario | Strategy |
|------|------|
| Player master data | Write-Behind (write Redis/memory first, asynchronously batch-flush to MySQL), with forced flush for critical operations |
| Currency changes | Dual write: immediate Redis update + TLog record + asynchronous DB flush; critical trades write the DB synchronously |
| Behavior logs | TLog only (Kafka) |
| Guild/social | Write-Through as needed (write DB first, then update cache) |

### 4.3 Caching strategy
- Redis Cluster: player snapshots, leaderboards, time-limited event state; key version numbers + TTL.
- Distributed locks/deduplication: use `SETNX` or RedLock to manage account login/operations.
- Cache update: the common pattern (invalidate + reload), or synchronization via subscribed messages (Pub/Sub).

## 5. storage.xml statement standard (reinforced 2026-09-29)

> The single statement source on the storage side. Symmetric with the contract side
> (`sdks/contract/`, docs/36 decision #3): **def governs on-wire transfer, storage.xml
> governs storage**. The reference shape is taken from the MyBatis mapper trio of
> "declarative statements + fail-fast full parsing at startup + parameterization"
> (full survey in docs/design/xml-generation.md §6); the parts explicitly not adopted
> are in §5.5.

### 5.1 Externalized statements (explicit statement declaration)

```xml
<?xml version="1.0" encoding="UTF-8"?>
<storage version="1" dialect="mysql8">
  <!-- Column mapping: explicit, one-way references to contract attr names
       (storage.xsd keyref locks out dangling references) -->
  <resultMap id="character_row" table="t_character">
    <column field="level" attr="level" type="int32"/>
    <column field="gold"  attr="gold"  type="int64"/>
    <column field="blob"  type="bytes" blob="true"/>  <!-- blob column gets no attr
       mapping; serialized as a whole -->
  </resultMap>

  <statement id="insert_character" kind="insert" resultMap="character_row">
    <param name="char_id" type="int64"/>
    <param name="level" type="int32"/>
    <param name="gold" type="int64"/>
    <param name="blob" type="bytes"/>
    <sql>INSERT INTO t_character (char_id, level, gold, blob)
         VALUES (#{char_id}, #{level}, #{gold}, #{blob})</sql>
  </statement>

  <statement id="journal_append" kind="insert">
    <param name="char_id" type="int64"/>
    <param name="seq" type="int32"/>
    <param name="payload" type="bytes"/>
    <sql>INSERT INTO t_journal (char_id, seq, payload)
         VALUES (#{char_id}, #{seq}, #{payload})</sql>
  </statement>

  <statement id="select_character" kind="select" resultMap="character_row">
    <param name="char_id" type="int64"/>
    <sql>SELECT char_id, level, gold, blob FROM t_character WHERE char_id = #{char_id}</sql>
  </statement>
</storage>
```

- **Statement four essentials**: id (globally unique, xs:key) + SQL template + parameter list + result mapping (for select/update backfill).
- **Parse into a prepared-statement pool at startup**: db-app fully parses storage.xml at startup and issues `PREPARE` for each statement; **any statement failing to parse, any `#{}` placeholder not matched by a `<param>` declaration, any dangling resultMap reference → startup fails** (aggregated error reporting, the same discipline as the contract parser).
- **Change SQL without recompiling**: SQL templates are data, not code — tuning index hints or rewriting an equivalent SQL only changes storage.xml plus a restart (or hot rebuild, §5.6).

### 5.2 The "what can change without recompiling" boundary

| Change | Recompile? | Why |
|---|---|---|
| SQL text (equivalent rewrite/index hint/dialect tweak) | **No** — prepared pool reloaded at startup | statements are data |
| `<param>` add/remove/type change | Yes — row struct and binding code regenerate | the types changed |
| `<resultMap>` column-mapping add/remove | Yes — generated artifacts follow the mapping | column↔field binding is a compile-time type |
| Table structure (DDL) | No (this file doesn't govern DDL; Flyway does, §9) | division of labor |

### 5.3 ResultMap goes through code generation; runtime reflection is banned

- The generator (same family as sdks/gen, an independent binary that **never enters the runtime link graph** — decision #4) reads storage.xml's resultMap + statement declarations and produces: a `row struct` (one strongly typed struct per resultMap) + by-name binding code (column name→field, type conversion done at the declaration layer) + statement id→strongly typed function ("look up statement by name" converges into "generated function by name"; a wrong name goes red at compile time).
- **Zero runtime reflection**: loading/writing back are compile-time-fixed strongly typed paths — no string→field guessing on hot paths (contrast the MyBatis Mapper dynamic proxy: C++ has no such mechanism, so generation directly produces functions, keeping the interface-binding idea).
- Generated artifacts follow the same discipline as `sdks/cpp/generated`: committed baseline + CI comparison (change storage.xml without regenerating → red).

### 5.4 Mandatory parameterization: only `#{}`, no `${}`

- At the syntax level, string-concatenation forms **do not exist**: storage.xsd recognizes only `#{identifier}` placeholders; `${` appearing in a SQL template fails validation (not a warning).
- `<param>` explicit declaration + startup check that the `#{}` set ⊆ the `<param>` set — a misspelled parameter name fails at startup, never surviving to runtime.
- The rationale traces to MyBatis's lesson: `${}` is an injection surface; the C-44 measured defect cluster (no escaping/conditional escaping/silent defaults) structurally disappears under "the only parameterized path".

### 5.5 MyBatis mechanisms explicitly not copied

| Not adopted | Reason |
|---|---|
| Dynamic SQL tags (`<if>/<foreach>/<where>` runtime interpreter) | The game-server statement surface is small and stable (write-behind three statements + column-promotion read/write + a few queries); query shapes are fixed, parameterized templates suffice; a mini-language interpreter is a liability |
| ORM lazy loading / N+1 / session-level cache (SqlSession/identity map) | In-memory-authoritative model (attribute-sync §3): in-process attributes are the truth, no row-by-row lazy fetching scenario; the write path goes through **write-behind journal** (log first, then snapshot, decision #6), not ORM transaction semantics as a fallback |
| Second-level cache `<cache/>` | Breaks single-writer and write-behind discipline (same for the Redis layer: cache only, no consistency duty) |
| Hot-swapping a single statement at runtime | Hot reload goes "full rebuild then swap" (see §5.6), structurally the same as MyBatis offering only the rebuild path |

### 5.6 Hot-reload form

FileWatcher/GM triggered → a worker thread fully re-parses storage.xml + all funnel validations → atomically rebuilds the prepared-statement pool at a tick boundary → on failure keep the old pool + alert. Not incrementally patching a single statement in the pool, but a whole rebuild-then-swap (structurally identical to the scripting-lua §3.2 table-swap protocol).

### 5.7 Zero-crossover boundary with the def contract (restated, decision #5)

- **attrs.xml knows nothing of storage.xml's existence**: the contract files contain no persistence fields/bits/declarations (SYNC_DB 0x04 is explicitly rejected by the parser; attribute names like `persist/column/table` are outside the whitelist) — switching databases, changing tables, tuning statements: zero contract changes, zero `schema_hash` change.
- **storage.xml references the contract one-way**: column mappings may reference attr names as the mapping source, direction always storage→contract; storage.xsd's xs:keyref locks out dangling references (referencing a nonexistent attr fails validation).
- Switching DB (MySQL→PostgreSQL) = swapping in another storage.xml statement set + dialect marker (`dialect`); contract, generator, SDK, and clients all untouched — a direct dividend of "learning from MyBatis: statements are data" (docs/36 Q11).

## 6. Sharding and Scaling

### 6.1 Sharding rules
- Initially: shard by account dimension (`char_id % N`); `N` from capacity estimates.
- Use ShardingSphere or develop routing in-house: DataProxy maintains the configuration with dynamic refresh.
- Horizontal scaling supported: after adding shards, migrate data via background tooling (online re-sharding).

### 6.2 Indexing and archiving
- Cold data (offline players, historical events) periodically archived to a cold store (another MySQL/ClickHouse).
- Archiving strategy: based on player lists produced from `last_login` and `server merge`.

## 7. TLog / BI Flow

### 7.1 Pipeline
```
GameServer → LogAgent → Kafka → (real-time) Flink → (offline) ClickHouse/Hive
```
- LogAgent uses TCP/NNG, guaranteeing write success (async + retry).
- Kafka topics divided by type: MoneyFlow, ItemFlow, LevelFlow, QuestFlow, etc.
- ClickHouse tables support near-real-time queries, providing GM/BI reports.

### 7.2 Log format
```
timestamp | platform | server | player_id | event | delta | balance | reason | extra_json
```
- Event enums synchronized with the Protobuf definitions; DataProxy can trigger TLog when writing data.

## 8. Data Recovery and Consistency
- **WAL/backup**: all asynchronous writes are recorded in the local WAL and replayed after a crash; the DB gets periodic full + incremental backups.
- **Hot standby**: MySQL master-slave replication + MGR; Redis Cluster multi-replica.
- **Consistency guarantee**: critical asset operations use transactions or a "respond only after the request lands" strategy; ordinary attributes use eventual consistency.
- **DR drills**: periodically rehearse DB failure, cache loss, Kafka lag, and similar scenarios.

## 9. Tooling and Process
- **Schema management**: use Flyway/Liquibase or an in-house tool to maintain SQL versions.
- **Data validation**: periodically compare Redis vs MySQL, checking for dirty data.
- **Data export**: provide player-data export/import tooling (GM debugging, troubleshooting).
- **Monitoring**: MySQL QPS/latency, Redis hit rate, DataProxy queue length, Kafka backlog.

## 10. Unified Decisions
- Official approach: adopt the Hybrid Schema + DataProxy + TLog system. Differences between `docs/07` and `docs/08` defer to this whitepaper.
- Blobs are used only for game-logic-sensitive data; fields BI/operations care about must be columnar.
- DataProxy is the single entry point: all services access the persistence layer through it; direct writes to the DB bypassing it are forbidden.
- **The single source of storage statements = storage.xml** (this file §5): externalized statements + mandatory parameterization + generated ResultMaps; zero bidirectional crossover between the def contract and storage.xml.

## 11. Roadmap
1. Finalize the `PlayerProfile` Protobuf and the columnar field list; generate C++/Lua/DB schema.
2. Implement DataProxy v1 (Load/Save + Redis cache + write queue).
3. Build the Kafka/ClickHouse TLog pipeline and define the log enums.
4. Complete sharding planning, migration tooling, and backup strategy.
5. Wire in monitoring/alerting and set operating procedures (data repair, rollback).
6. The db-app batch (todo batch 2): storage.xsd + storage.xml v1 + statement-pool loading + row-struct generation wiring (§5).

---

This whitepaper provides the unified standard for subsequent DataProxy, DB design, and BI integration.***
