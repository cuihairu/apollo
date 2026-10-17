# MMO Mechanism Source Analysis

External-source research covering transport, entities, scripting, persistence, simulation, visibility, lifecycle, operations, and identity. These mechanisms do not define Apollo's process topology or feature commitments.

## 0. Evidence and source baseline

Grade A uses source paths and line numbers; B uses repository documents; C uses official external documentation; D is community material requiring qualification.

The inspected source baselines are BigWorld `27446bab` (14.4.1), KBEngine `0bc93d5`, and skynet `4f76d75`. References describe those copies, not a claim about every release. Source absence claims apply only to their stated search scope. Actual paths/lines provide provenance instead of references to removed review sections.

Apollo's current boundaries follow its [architecture status](./architecture-review.md), [SDK contract](../design/sdk-contract.md), and [cross-server design](../design/cross-server-scenes.md).

---

## §1 Network Layering and Frame Pipeline (36 #1: self-built four-layer network)

**Implementation locations (grade A, source baseline)**

| Mechanism | BigWorld (Mercury) | skynet |
|---|---|---|
| Event multiplexing | `lib/network/event_poller.hpp:95-137` abstract base (doRegisterForRead/Write + processPendingEvents pure virtual); **the only backend is select**: `lib/network/event_poller.cpp:496` `select( fdLargest_+1, … )` | the socket thread owns epoll exclusively: `skynet-src/socket_epoll.h:20` (verified §5.8; the kqueue variant socket_kqueue.h coexists) |
| Frame delimiting | Bundle fragmentation: `lib/network/udp_bundle.cpp:567`  | gate length header: `service-src/service_gate.c:353` (sscanf of startup args)/:362 (header allows only `'S'`/`'L'`); 2-byte path big-endian length read `lualib-src/lua-netpack.c:184-185` (`buffer[0]<<8 \| buffer[1]`) |
| Filter plugin slot | `lib/network/udp_channel.hpp:81` constructor-injected `PacketFilterPtr`, `:139-140` accessors ; the two-family interface `packet_filter.hpp:37/:48/:56`, `stream_filter.hpp:46/:55` | no filter layer — WS framing lives in the Lua service assembly layer (`lualib/http/websocket.lua`, verified source baseline) |
| Dual protocol | `lib/network/channel.hpp:104` `isTCP()`  | TCP only (gate) |

**Data structures and key fields (grade A)**
- skynet gate (`service_gate.c:15-35` source baseline): `struct connection { int id; uint32_t agent; uint32_t client; char remote_name[32]; struct databuffer buffer; }`; `struct gate { ctx, listen_id, watchdog, broker, client_tag, header_size, max_connection, struct hashid hash, struct connection *conn, messagepool }` — **the connection table is an array + hashid index**, one streaming `databuffer` per connection.
- BW channel-lifetime data structures: `lib/network/condemned_channels.hpp` / `delayed_channels.hpp` (directory-level evidence, ls the stated baseline) — closing channels get their own table so the wind-down period is unaffected by new channels.
- BW UDP window: `lib/network/udp_channel.cpp:92-108` sliding window / `:866` stop on over-window / `:1052` resend-driver kick .

**State machine/sequence**
- skynet gate receive path: the socket thread gets fd data → gate service message → netpack `filter` (`lua-netpack.c:191-241` source baseline, uncomplete logic: half-packets held in `struct uncomplete`, sent only once `pack_size` is complete) → forwarded per the conn's agent/client (`service_gate.c:169` `_forward`).
- BW Mercury main loop: `EventDispatcher::processNetwork` (`lib/network/event_dispatcher.cpp:366-374` source baseline) → `pPoller_->processPendingEvents( maxWait )` — readiness notification + earliest timer deadline as the max wait.

**Config parameter defaults**
- skynet: `skynet-src/skynet_main.c:160` `optint("thread",8)` — worker threads default 8 (`:161-162` SKYNET_MAXTHREAD cap check); gate `max_connection` is a mandatory startup arg (`service_gate.c:356-360`, no default).
- BW: the poller has no config item (select chosen at compile time); channel-window parameters per the source baseline-verified entries.

**Known defects**
- BW select-only: fd count is limited by FD_SETSIZE; scaling means swapping the backend — which hasn't happened in twenty years (evidence for §5.8's "the mainstream stays in the readiness family").
- skynet gate: frame delimiting (2B/4B) and the message body (sproto) are coupled in the netpack/gate assembly, unlike BW's unified filter slot — adding encapsulation means modifying gate/netpack themselves (stated in the architecture-review 16.8.2 table).
- The implementation-level basis for 36 #1's verdict "take only the filter-slot semantics, define the frame format ourselves" is the table above: the filter injection point (udp_channel.hpp:81) is portable; UDP+aggregated-packet binding is not.

---

## §2 Generational Replacement of the Inter-Process Messaging Layer (36 #2: nng retirement)

**Implementation locations (grade A, source baseline)**
- harbor (first generation, C service): `skynet-src/skynet_handle.c:30-40` `struct handle_storage { rwlock; uint32_t harbor; uint32_t handle_index; slot…; name… }`; `:110` `handle |= s->harbor;` — **the handle's high bits carry the harbor id** (harbor pre-shifted to the high bits, 8-bit node number; verified source baseline at skynet_handle.c:33,111). The service header comment `service-src/service_harbor.c:6-14` self-describes the text-protocol command family: `N name` (update a global name) / `S fd id` (actively connect a new node: send own id first, receive and verify the peer's id, then send the queue) / `A fd id` (passive accept) / `D id` (disconnect report) / `Q name` (unknown global-name query).
- cluster (second generation, a Lua service family): `service/clusterd.lua:40-50` (node_sender table + double-check construction of clustersender); sends go through `service/clustersender.lua:27/:29/:52` `channel:request(...)` (request/response over a socketchannel); config loading `clusterd.lua:88-108` loadconfig (`__` prefix marks options).
- Constants: `service_harbor.c:24` HASH_SIZE 4096, `:25` DEFAULT_QUEUE_SIZE 1024, `:28` HEADER_COOKIE_LENGTH 12.

**Data structures and key fields**: the harbor side = handle_storage (above); the cluster side = clusterd's `node_sender{name→clustersender}` + the socketchannel held by clustersender.

**State machine/sequence**: harbor connection handshake (S/A commands, exchange ids first, then drain the queue) → on disconnect send the `D id` text message to the slave. cluster: clusterd lazily builds a clustersender per node name → TCP socketchannel → request/padding sends.

**Config parameter defaults**: harbor id comes from startup args (`skynet_handle_init(int harbor, int thread)`, `skynet-src/skynet_handle.h:20`); the cluster node table comes from the clustername config file (clusterd loadconfig).

**Known defects**
- 8-bit harbor = at most 255 nodes; master/slave mode is complex (the cmaster/cslave service families are still in `service/`).
- **Two generations coexist in one framework** (verified source baseline, corroborated by 16.5-③) — the direct argument for 36 #2: generational replacement of self-built messaging layers is the norm; adopting nng outsources the replacement risk.
- The local skynet `docs/architecture/harbor-cluster.md` is a "being written" placeholder (sed of its header, the stated baseline) — **the upstream docs give no authoritative line on the harbor deprecation timeline; honestly marked as not found**; the code facts (two generations coexisting, cluster being a full Lua family) suffice to support the #2 verdict.

---

## §3 Entity Contract Declaration (.def parsing) (36 #3: XML+XSD)

**Implementation locations (grade A)**
- BW: `lib/entitydef/entity_description.cpp:184-190` (BWResource::openSection parses .def), `:194-203` (`<Parent>` expanded recursively at parse time, parents before children), `:406` (`volatileInfo_.parse( pSection->openSection( "Volatile") )`, source baseline); field-flag parsing `lib/entitydef/data_description.cpp:220` (Persistent)/`:228-233` (Identifier implies Indexed+Unique)/`:238-250` (Indexed)/`:314` (DatabaseLength) . Sample `examples/client_integration/python/simple/res/scripts/entity_defs/Guard.def:2-6` (source baseline):
  ```xml
  <Volatile>
      <position/>
      <yaw/>
      <pitch>	20	</pitch>
  </Volatile>
  ```
- KBE: `kbe/src/lib/entitydef/entitydef.cpp:188-210` (tinyxml2 reads entities.xml first, then per-entity .def; verified source baseline, spot-checked consistent the stated baseline); DetailLevels parsing `:409-417` (NEAR/MEDIUM/FAR each with radius/hyst child nodes, source baseline).

**Data structures and key fields (grade A, source baseline)**
- KBE `kbe/src/lib/entitydef/property.h:23` `class PropertyDescription : public RefCountable` — **the constructor parameters are the complete set of .def property fields**: `dataTypeName, name, uint32 flags, bool isPersistent, bool isIdentifier, std::string indexType, uint32 databaseLength, std::string defaultStr, DETAIL_TYPE detailLevel` (the :27-40 region, with the getter family isPersistent()/getFlags() etc.).
- BW `lib/entitydef/volatile_info.hpp:13-47` `class VolatileInfo { float positionPriority_, yawPriority_, pitchPriority_, rollPriority_; }` — per-axis priority floats, `shouldSendPosition() = positionPriority_ > 0` (:23), static constant `ALWAYS` (:36).
- KBE `kbe/src/lib/entitydef/detaillevel.h:21-23`: `struct Level { float radius; float hyst; }`, default constructor `radius(FLT_MAX), hyst(1.0f)`.

**State machine/sequence**: no runtime state machine — pure startup parsing (exactly the counter-image against which apollo moves parsing+validation into CI).

**Config parameter defaults**: BW VolatileInfo defaults to -1 (don't send); KBE Level defaults radius=FLT_MAX/hyst=1.0 (above).

**Known defects**
- **Both parse only, with no schema layer** (C-50): a mistyped tag silently defaults — tinyxml2 has no validation (direct proof at entitydef.cpp:193), BW XMLSection likewise (source baseline).
- Inheritance has no cycle guard: BW `<Parent>` has no cycle detection (source baseline: rg for cycle/recursi/visited — zero hits); KBE's loadParentClass↔loadDefInfo mutual recursion has no depth guard (entitydef.cpp:890-916/:372, verified source baseline).

---

## §4 Generator and Tool-Tree Layout (36 #4: sdks/ layout, generator as a standalone binary)

**Implementation locations (grade A, directory census the stated baseline)**
- KBE tools outside the source tree: `kbe/tools/{server, xlsx2py}`; `kbe/tools/xlsx2py/xlsx2py/` contains `xlsx2py.py / ExcelTool.py / config.py / functions.py / py2excel.py / itemchar.py / petchar.py / syschar.py / character.py / xlsxtool.py / xlsxError.py` — a complete Excel → runtime py-config tool family (gb2312-encoded headers, xlsx2py.py:1 source baseline).
- KBE SDK templates and delivery: `kbe/res/sdk_templates/{client,server}`; SDK delivery is part of the login chain: `kbe/src/server/loginapp/clientsdk_downloader.{h,cpp}` (existence verified source baseline, path re-checked the stated baseline).
- KBE ops tool processes: `kbe/src/server/tools/{bots, guiconsole, interfaces, kbcmd, logger}`.
- BW standalone tool-binary family: `server/tools/{bwmachined, bw_profile, message_logger, sync_db, snapshot_helper, consolidate_dbs, clear_auto_load, remove_db, transfer_db, bots}` (ls the stated baseline) — `sync_db` is the contract-adjacent ".def change → database schema sync" tool.

**Data structures and key fields**: xlsx2py produces one py module per sheet (ExcelTool.py loads/parses; config.py holds paths and switches) — the stated baseline only read file headers and directories; **did not line-by-line verify ExcelTool's parsing logic, honestly marked**.

**State machine/sequence**: N/A (build-time tooling).

**Config parameter defaults**: xlsx2py's config.py is user-editable configuration (not expanded or compared the stated baseline; no specific defaults cited).

**Known defects**: Excel→py has no schema validation (type errors blow up only at runtime); BW entitydef goes through runtime parsing rather than generation time (16.8.3-② already stated as the counter-image of apollo's deliberate divergence).
**Implementation-level basis for the #4 verdict**: in both engines, the "source→artifact" converters sit entirely outside the runtime link graph (kbe/tools outside the source tree, BW server/tools standalone binaries) — apollo sdks/gen is the same shape.

---

## §5 The Contract–Storage Coupling Surface (36 #5: storage.xml, two segments)

**Implementation locations (grade A)**
- KBE: `kbe/src/lib/db_mysql/entity_table_mysql.cpp:699-758` (ARRAY/FIXED_DICT/PYTHON/ENTITYCALL/Component → FIELD_TYPE_BLOB), `:236-300` (`<Indexed>` properties build real indexes), `:113` (ALTER TABLE ADD INDEX) .
- BW: `lib/db_storage_mysql/column_type.cpp:95-129` (Indexed/Persistent → MySQL column-type mapping; verified source baseline); the rest of blob serialization in `lib/db_storage_mysql/mappings/{blob,class,composite}_mapping.cpp` ; the MySQL backend directory `lib/db_storage_mysql/{buffered_entity_tasks.*, bw_meta_data.*, column_type.cpp, …}`, the XML backend `lib/db_storage_xml/xml_database.cpp` (ls the stated baseline).

**Data structures and key fields**: the source flags of KBE's column types are exactly PropertyDescription's `isPersistent/isIdentifier/indexType/databaseLength` (§3, source baseline) — **contract fields and storage columns live in the same constructor parameter list**; that is the implementation form of the "coupling". BW is the same shape (DataDescription's Persistent/Indexed flags, data_description.cpp verified source baseline).

**State machine/sequence**: at startup (or via the sync_db tool) table schemas are generated/synced from .def; no runtime migration mechanism.

**Config parameter defaults**: KBE `kbengine_defaults.xml` database sections (the stated baseline only verified the cellapp/baseapp sections; DB-section defaults not expanded — not cited).

**Known defects**: of C-51's five claims, 1/3/5 hold (blob-ification/no journal/no hot-cold tiering); 2/4 were corrected per source baseline (there is an Archiver with periodic archival, and Indexed has real indexes). **Implementation-level basis for the #5 verdict**: both engines' persistence flags grow on the contract-description struct (PropertyDescription/DataDescription); apollo replaces "field-level coupling within the same struct" with the physical file separation of storage.xml.

---

## §6 Backup/Archive Smoothing and Amortized Writes (36 #6: write-behind journal)

**Implementation locations (grade A, source baseline)**
- BW BaseApp backup: `server/baseapp/backup_sender.hpp:52-79` (field list below) + `server/baseapp/backup_sender.cpp:102-121` (tick main flow and fractional-remainder smoothing); defaults `server/baseapp/baseapp_config.cpp:23` `BW_OPTION( float, backupPeriod, 10.f )`, `:84` `backupPeriodInTicks` (secondsToTicks conversion).
- KBE Archiver: `kbe/src/server/baseapp/archiver.cpp:26-63` (source baseline, including the author's Chinese-commented algorithm note: `baseEntity的数量 * idx / tick周期 = 每次在vector中移动的一个区段` ["the number of base entities * idx / tick period = the vector segment advanced each time"], the formula landing at `:39 startIndex = size * archiveIndex_ / periodInTicks` / `:43 endIndex`); destroy-to-database at `kbe/src/server/baseapp/entity.cpp:698-731` (onDestroyEntity → assembles a DbmgrInterface::removeEntity Bundle and sends it to dbmgr).
- KBE write path: SQL statements registered per table into a mapping table `kbe/src/lib/db_mysql/entity_sqlstatement_mapping.h:9-27` (verified source baseline-②).

**Data structures and key fields (grade A, source baseline)**
- BW `BackupSender` (backup_sender.hpp:52-79): `int offloadPerTick_` / `float backupRemainder_` (**fractional remainder carried across ticks**) / `BasesToBackUp basesToBackUp_` / `BackupHash entityToAppHash_, newEntityToAppHash_` (consistent-hashing dual tables: current + switching) / `bool isUsingNewBackup_, isOffloading_` / `int ticksSinceLastSuccessfulOffload_`.
- KBE `Archiver`: `arEntityIDs_` (**a shuffled sequence of entity ids**, initialized at archiver.cpp:13, cleared and rebuilt at :69) + the `archiveIndex_` cursor.

**State machine/sequence**
- BW: normal period (tick → compute numToBackUp → send to each backup baseapp per the hash table) → hash-switch period (old and new tables both used until ackNewBackupHash) → retirement period (isOffloading_, offloadPerTick_ only grows, until everything migrates).
- KBE: archiveIndex_ +1 per tick → entities within the [startIndex,endIndex) segment written to DB one by one → when index ≥ periodInTicks, createArchiveTable and rebuild the shuffled sequence.

**Config parameter defaults**: BW backupPeriod **10 seconds** (baseapp_config.cpp:23); KBE archivePeriod **300 seconds** (kbengine_defaults.xml:621), backupPeriod **300 seconds** (the :625 region, same section source baseline).

**Known defects**
- KBE archiving is **not dirty-driven** (round-robins entities, unaware of attribute dirt — the corrected phrasing of C-51 item 2); the default 300s window is coarse.
- BW backup is a **snapshot stream** (base state sent whole), no journal/redo — crash-recovery granularity equals the backup period (the reference surface for the G-2 argument).
- Both smoothing algorithms share a shape with different implementations: BW float remainder (backup_sender.cpp:114-117 source baseline: `numToBackUpFloat = bases.size()/periodInTicks + backupRemainder_; … backupRemainder_ = the fractional part`), KBE segment cursor — **this is the implementation-level basis for 36 #6's "KBE's smoothing algorithm rewritten by apollo as a head segment of dirty entities"**.

---

## §7 Per-Observer Watermarks (36 #7: per-viewer acked_seq)

**Implementation locations (grade A, source baseline)**
- `server/cellapp/witness.cpp:2470-2516`: the dumpAoI **official doc-comment block** — field-by-field explanation of the output line `16777218: volatile 129/130. event 45/45. priority !0.000000`: the observer has received volatile update #129, the entity has produced #130; events #45/#45; priority 0.000000 and prioritised. First line `Seen=2 AoIMap=2 AoIRadius=500.000 AoIHyst=5.000`.
- `server/cellapp/witness.cpp:1248-1258` (source baseline): after pop_heap, reset `pCache->priority(startingPriority)`, `updatePriority(entity_.position())`, `clearPrioritised()` — the maintenance point of the **per-(viewer,entity) priority heap**.
- `server/cellapp/witness.hpp:26` `class Witness : public Updatable`, `:158-161` setAoIUpdateScheme/getAoIUpdateScheme, `:275` `EntityCacheMap aoiMap_` (source baseline).
- Client-reported update rates: `server/cellapp/server_connection.cpp:2370-2373`, per-client byte budget `:2034` .

**Data structures and key fields (grade A, source baseline at entity_cache.hpp:80-101/:184-189)**
```cpp
EventNumber     lastEventNumber_;          // int32  —— event sequence the client has received
VolatileNumber  lastVolatileUpdateNumber_; // uint16 —— volatile sequence the client has received
EventNumber     lodEventNumbers_[ MAX_LOD_LEVELS ]; // int32 × LOD level count
```
plus the `Priority priority()` pair (:80-81). **These are the entity fields behind 36 #7's "volatile+event dual sequence numbers"**.

**State machine/sequence**: each tick the witness takes the cache at the top of the priority heap → if the entity's volatile/event sequence has advanced, send the delta → pop_heap reorders.

**Config parameter defaults**: AoIRadius/AoIHyst settable per entity (dumpAoI sample 500/5); AoIUpdateScheme in §11.

**Known defects**: witness and its EntityCache all live in `server/cellapp/` (directory-level evidence) — **bound to the CellApp process**; the implementation location makes directly visible the motive for 36 #7's "apollo generalizes the watermark into an independent AOI service".

---

## §8 Fixed Tick and Thread Fleet (36 #8: 20Hz main thread + IO pool)

**Implementation locations (grade A)**
- BW: `lib/server/common.hpp:19` `const long DEFAULT_GAME_UPDATE_HERTZ = 10;` + `lib/server/server_app_config.cpp:28` (BW_OPTION registration, **default 10Hz** — source baseline); `server/cellapp/cellapp.cpp:806-808` `addTimer(1000000/updateHertz, TIMEOUT_GAME_TICK)`, `:946-948` handleGameTickTimeSlice, `:1177-1190` the three-way updateLoad accounting ; main loop `lib/server/server_app.cpp:240-242` processUntilBreak .
- KBE: `kbe/res/server/kbengine_defaults.xml:5` `<gameUpdateHertz> 10 </gameUpdateHertz>` (**default 10Hz**); `kbe/src/server/cellapp/cellapp.cpp:251-261` handleGameTick (source baseline: the `updateLoad()` comment self-states "must be at the very front" → `EntityApp<Entity>::handleGameTick()` → `updatables_.update()` → `SpaceMemorys::update()`).
- skynet (the counter-example side): `skynet-src/skynet_main.c:160` `optint("thread",8)` (workers default 8); the thread fleet monitor+timer+socket+worker `skynet-src/skynet_start.c:209-227` (verified source baseline; the weight table re-read the stated baseline: `{-1×4, 0×4, 1×8, 2×8, 3×8}`); weight-strided dispatch `skynet-src/skynet_server.c:316-318`, per-message requeue `:293-315` ; overload threshold `skynet_mq.c:19` MQ_OVERLOAD 1024 .

**Data structures and key fields**: BW ScriptTimeQueue/EntityAppTimeQueue (lead positions source baseline at `lib/server/entity_app.cpp:25-49`); skynet global queue + per-service private mq (spinlock, skynet_mq.c:22 verified source baseline).

**State machine/sequence**: BW/KBE = fixed-length tick timer drives game logic, messages processed on arrival (both coexist); skynet = purely message-driven with no tick, empty queues cond_wait (skynet_start.c:167-174 verified source baseline).

**Config parameter defaults**: the three default 10 / 10 / 8 threads (above).

**Known defects**: skynet's weight striding makes starvation risk of heavy services explicit (weight 0 = yield after every message); under BW/KBE fixed ticks, message processing and ticks contend for the same thread (the time-slice dispatch exists for exactly this).
**Implementation-level basis for the #8 verdict**: KBE's 10Hz fixed tick (defaults xml:5) + skynet's single-service serialization (skynet_server.c:293-315) — two pieces of evidence mapping to apollo's "main thread total order + pooled bypass" hybrid sourcing.

---

## §9 Seamless World and Ghosts (36 #9: scene-instance system)

**Implementation locations (grade A, census the stated baseline)**
- The BW ghost file family (`server/cellapp/` directory census): `buffered_ghost_message{.cpp,.hpp}` / `buffered_ghost_message_factory.*` / `buffered_ghost_message_queue.*` / `buffered_ghost_messages.*` / `buffered_ghost_messages_for_entity.*` / `entity_ghost_maintainer.{hpp,cpp}` / `real_caller.*` / `real_entity.{hpp,cpp}`.
- `server/cellapp/entity_ghost_maintainer.hpp` class comment (excerpt, source baseline): *"This class is used to visit all the cells of a space, and create new ghosts for those that are required for a particular entity, as well as unmark those that are still required to exist."*

**Data structures and key fields**: the ghost message-buffer family (buffered_ghost_message_queue/for_entity) — **method calls arriving before a ghost is fully built enter a buffer queue first, and are delivered once built**; real_caller/real_entity maintain "the ghost→real call forwarding".

**State machine/sequence**: an entity nears a cell boundary → EntityGhostMaintainer visits all cells of the space → creates ghosts in the cells that need them → unmarks/reclaims after moving away; meanwhile calls to that entity are forwarded to the authoritative side via real_caller.

**Config parameter defaults**: no independent ghost-threshold config item was located the stated baseline (BW drives it by cell boundaries/AoI radius; no separate default seen — **honestly marked: no independent parameter found**).

**Known defects**: ghost double writes, boundary negotiation, cross-process debugging without single-stepping — from a deleted grade-B doc (`git show 1e37073d^:docs/BigWorld架构深度解析.md` §五, cited by 36 #9); implementation-side corroboration: the very existence of the buffer-queue family is the "complexity tax of cross-process consistency".
**Implementation-level basis for the #9 verdict**: the entire file family above serves "cross-cell view consistency", a layer apollo's instance system does not need.

---

## §10 AOI Structure (36 #10: AOI as an independent service)

**Implementation locations (grade A, source baseline)**
- KBE cross-linked lists: `kbe/src/server/cellapp/coordinate_node.h:94-99` six-way list accessors (`pPrevX/pNextX/pPrevY/pNextY/pPrevZ/pNextZ`, :104-109 the corresponding setters); `coordinate_system.{h,cpp,inl}` maintain the three ordered axis lists; `entity_coordinate_node.{h,cpp}` the entity nodes.
- BW: no standalone AOI data-structure module — AOI = the witness's aoiMap_ (§7) + trigger radii (AoIRadius/AoIHyst) + AoIUpdateScheme (§11).

**Data structures and key fields**: CoordinateNode (coordinates + six-way list pointers, above); CoordinateSystem holds the three axis-list heads. **Cross-linked lists (linked lists on axes)**: moving = O(position-in-list) updates; neighborhood queries = truncating the X/Y lists by radius.

**State machine/sequence**: entity add → ordered insert on three axes → move → local re-linking + neighborhood callbacks → remove → unlink.

**Config parameter defaults**: KBE query radius comes from DetailLevels' radius (detaillevel.h:21 defaults to FLT_MAX, see §3/§11).

**Known defects**: **the files all sit in `kbe/src/server/cellapp/` (directory-level evidence)** — the structure is welded to the cellapp process, exactly what 36 #10's "locked inside cellapp, apollo strips it out as a shard service" verdict points at; the BW side likewise (witness lives in cellapp).

---

## §11 Visibility Tiering and Conditional Sync (36 #11: SYNC_* bitmask)

**Implementation locations and evidence tiers**
- BW (grade A, source baseline): per-axis volatile priorities — `lib/entitydef/volatile_info.hpp:16-47` (four-axis float priority + the `ALWAYS` constant + `shouldSendPosition() = priority > 0`); the .def sample Guard.def:2-6 (`<pitch>20</pitch>` is priority 20); **distance tiering**: `server/cellapp/aoi_update_schemes.hpp:18-56` `class AoIUpdateScheme { float weighting_, distanceWeighting_; }`, `apply(distanceSq) = (sqrt(d)*distanceWeighting_ + 1) * weighting_` (weight 0 gives a coincident constant 1.0); a static 256-bucket table (`schemes_[256]`) + name mapping; **scheme id hung per EntityCache** (entity_cache.hpp:98-99/:177) + **a per-LOD event-sequence array** `lodEventNumbers_[MAX_LOD_LEVELS]` (entity_cache.hpp:189, accessors :101/:164-165).
- KBE (grade A, source baseline): `kbe/src/lib/entitydef/detaillevel.h:21-23` `struct Level{radius=FLT_MAX, hyst=1.0}` + `entitydef.cpp:409-417` (the DetailLevels NEAR/MEDIUM/FAR radius/hyst XML children) + the tiering implementation in `detaillevel.cpp/h`.
- UE `COND_*` (grade C): official doc links in 36's #11 row (Conditional Property Replication) — no UE source locally, **no line numbers cited**.

**Data structures**: as above (AoIUpdateScheme dual weights / Level radius+hysteresis / the LOD sequence array).

**State machine/sequence**: BW: distance → scheme.apply() → the priority delta enters the heap (the heap in §7 is the consumer) — **"dense when near, sparse when far" is realized continuously by the priority function, not discrete tiers**; KBE: radius tiering + hysteresis to damp boundary flapping (lag entering/exiting tiers).

**Config parameter defaults**: BW priority defaults -1 (don't send), the ALWAYS constant, schemes capped at 256; KBE radius defaults FLT_MAX, hyst 1.0.

**Known defects**: UE COND_* is a compile-time macro (stated in 36 #11); BW/KBE tier declarations ride .def/engine internals, not configurable cross-endpoint — hence apollo's instance to reshape via the `sync=` contract field.
**Implementation-level basis for the #11 verdict**: BW's LOD sequence array + the scheme weight function = a mature implementation of "tiered sync by distance/importance"; KBE's three-tier radius + hysteresis = the simple workable version.

---

## §12 Script Embedding Form (36 #12: whitelisted Lua scripting — revised 2026-09-30: drop sol2 for native C API bindings; version = 5.5 mainline following the vcpkg lua port, currently 5.5.x)

**Implementation locations (grade A, source baseline)**
- KBE Python embedding: `kbe/src/lib/pyscript/script.cpp:76` (`Py_InitializeFromConfig`)/`:229` (the `Py_Initialize` compatibility path) — **full-featured Python, no sandbox**; `:383` the engine itself does `PyImport_ImportModule("os")` (for system info) — the os module is directly reachable from the script layer.
- skynet hot-reload: `service/debug_console.lua:159` (the command table registers `inject`)/`:270` (`COMMAND.inject(address, filename, ...)`); implementation `lualib/skynet/debug.lua:85-87` (`require "skynet.inject"` then executes inside the target service's context) — **inject new code into a designated service online**; the official docs/architecture/hot-reload.md is a placeholder (source baseline; the code is authoritative for mechanism details).
- skynet coroutine feel: `lualib/skynet.lua:227` `coroutine_yield "SUSPEND"` (re-verified consistent the stated baseline).

**Data structures and key fields**: KBE's `Script` class holds the `__main__` module and extraModule_ (script.cpp:161/:240-243 source baseline); skynet inject replaces function definitions file-by-file (debug.lua consumes the dispatch/register_protocol references).

**State machine/sequence**: KBE: process start → Py init → register the KBEngine module → load scripts; **no server-side script hot-reload mechanism seen within the range read** (honestly marked: no reload/inject counterpart found in kbe/src). skynet: the console's inject command → the target service executes the injected code inside its own message loop.

**Config parameter defaults**: KBE python configuration in kbengine_defaults.xml (entryScriptFile etc.; the bots-section sample `kbemain` seen the stated baseline; not expanded item-by-item).

**Known defects**: KBE's Python attack surface (os reachable, script.cpp:383) + no hot reload (process restart); skynet inject is function-level replacement with no version/rollback protocol (the increment over it is exactly apollo scripting-lua §3.2's smoke-test + rollback protocol).
**Implementation-level basis for the #12 verdict**: both pieces of evidence in place — KBE as the negative (full-featured + not hot-reloadable), skynet as the positive (Lua can carry business logic + hot-reloadable).

---

## §13 Process Discovery (36 #13)

**Implementation locations (grade A, source baseline)**
- KBE machine: `kbe/src/server/machine/machine.cpp:646-670` `findBroadcastInterface` — `bhandler.broadcast(KBE_PORT_BROADCAST_DISCOVERY)` sends a broadcast probe → `receive` collects replies → confirms the default broadcast route interface (line-for-line consistent with the source baseline record).
- **KBE's own config comment admits the defect** (source baseline, `kbengine_defaults.xml:814-833` machine section): *"在某些网络环境由于路由器的设置不允许 UDP 广播造成跨物理机组网不成功时，可在此填入所有相关物理机的地址，引擎将会向具体的地址发送探测包来完成组网"* ["in some network environments where router settings disallow UDP broadcast and cross-machine clustering fails, you may list the addresses of all relevant physical machines here; the engine will send probe packets to those specific addresses to complete clustering"] (the `<addresses>` manual-list workaround).

**Data structures and key fields**: the broadcast handler (UDP socket + the interface table `map<u_int32, string> interfaces`, machine.cpp:661-666 source baseline); the machine exposes the port range `externalTcpPorts_min=20099` (defaults xml, source baseline).

**State machine/sequence**: process start → broadcast/shake to the machine → the machine maintains the component table (Components) → death detection (timeout cleanup).

**Config parameter defaults**: externalTcpPorts_min 20099 / max 0 (=auto); `<addresses>` empty by default.

**Known defects**: **broadcast failing on routers/cloud networks is admitted by KBE's own config comment** (quoted above); same-subnet limitation is a physical property of UDP broadcast.

---

## §14 Ops Observability (36 #14: external standard stack)

**Implementation locations (grade A, source baseline + verified source baseline)**
- skynet's three axes: monitor version-number stall detection `skynet-src/skynet_monitor.c:31-45` ; the debug_console command table `service/debug_console.lua:147-174` (full table source baseline: stat/info/exit/kill/mem/gc/start/snax/clearcache/service/task/uniqtask/**inject**/logon/logoff/log/debug/signal/cmem/jmem/ping/call/trace/netstat/**profactive**/**dumpheap**/killtask/dbgcmd — including the jemalloc heap-profiling switches); the standalone logger service `service-src/service_logger.c:9-27` (source baseline: `struct logger { FILE* handle; char* filename; uint32_t starttime; int close; }`).
- BW centralized logging: `server/tools/message_logger/` (directory census the stated baseline, ~70 files: the log_storage/query engine family + a **mongodb/ subdirectory** (MongoDB backend) + mldb + the py_bwlog Python query interface) — log querying built into a standalone system with its own query language; profiling `server/tools/bw_profile`; the reporting end logger_endpoint (verified §5.7 at lib/network/logger_endpoint.cpp:672-703).
- KBE: the standalone logger process `kbe/src/server/tools/logger/logger.{cpp,h}` (existence source baseline); the watcher path tree `kbe/src/lib/server/serverapp.cpp:165-181` + remote query `:181-198` ; telnet online execution `kbe/src/server/*/telnet_handler.cpp:801-812` ; the GUI consumer guiconsole (kbe/src/server/tools/guiconsole).

**Data structures and key fields**: the logger structs above; the debug_console command table is itself "the ops-surface API inventory".

**State machine/sequence**: the monitor thread compares version numbers per dispatch (unchanged = stuck, alarm without killing, skynet_monitor.c:31-45); the logger service receives PTYPE_LOG and writes to disk.

**Config parameter defaults**: the logger disk parameters of each were not expanded the stated baseline (not cited).

**Known defects**: the shared form across all three is "detection primitives in-process, aggregation and display in external tools" (the 16.8.2 table); the defect is **self-built wheels not plugging into the ecosystem** (36 #14's "right direction, dated form") — BW's message_logger with its own query engine + MongoDB backend is the extreme case of "old but heavy".

---

## §15 The Entity/Process Identity System (36 #15) — **this document's correction row for 36**

**Grade-A facts (source baseline)**
- `lib/network/basictypes.hpp:104` `typedef int32 EntityID;` — **BW's entity id is 32-bit** (unique within the process, pre-persistence).
- `lib/network/basictypes.hpp:191` `typedef int64 DatabaseID;` (with `PENDING_DATABASE_ID = -1`) — **the 64-bit one is the database id**, assigned by the db side, **no segmentation semantics seen**.
- `lib/cstdmf/unique_id.hpp:17-25` `class UniqueID { uint a_,b_,c_,d_; }` — 128-bit, four-segment dotted-decimal form (header comment *"4 uints represented in hex separated by periods"*), used for component identity in machined/logging etc. (companion mapping `lib/db_storage_mysql/mappings/unique_id_mapping.cpp`).
- A process's network-layer identity = Mercury `Address` (ip:port).

**External-engine comparison**: a prior illustrative segmented sketch `struct EntityID { uint64 id; type(8 bits)|serverId(16 bits)|index(40 bits) }` — **that structure does not exist in BW source** (rg found no counterpart); it was the doc author's illustrative pseudocode.

**Conclusion (correcting 36 #15)**: the trace target of 36's "BW 64-bit unique ID (type|serverId|index segments)" **has no source-code counterpart** — it should be rewritten as "BW's actual identity system = EntityID(int32) / DatabaseID(int64) / UniqueID(128-bit dotted) / Mercury Address; **the segmented 64-bit ServerID is apollo's own design**, with no BW implementation counterpart; what can be cited is only the idea of 'embedding provenance inside the ID for traceability' (the int64 capacity of the DatabaseID space and the structured segmentation of UniqueID are two referenceable precedents)". The apollo design itself is unaffected; **the trace statement needs correction**.

---

## §16 Standalone Battle Instances and Replay (36 #16) — negative-space confirmation of a self-created design

**Grade-A negative-space evidence (source baseline)**: BW's `server/` process inventory = `{baseapp, baseapp_extensions, baseappmgr, cellapp, cellapp_extensions, cellappmgr, dbapp, dbapp_extensions, dbappmgr, loginapp, reviver, tools}` — **no standalone battle process**; KBE's `kbe/src/server/` = `{baseapp, baseappmgr, cellapp, cellappmgr, dbmgr, loginapp, machine, tools}` — same; skynet has no process-topology concept at all (single node, many services).
**Conclusion**: the reviewed repositories do not supply this exact combined mechanism. Apollo’s current World and Instance boundaries are defined in the architecture overview; this external comparison does not add a target process role.
Known defects (as the risk surface of a self-created design, not an external defect): the party/return flow of standalone battle processes and cross-process entity-projection consistency are problems apollo must solve itself — no precedent among the three to copy.

---

## §17 Database Support Matrix (36 #17) — **this document's correction row for 36**

**Grade-A facts (directory census source baseline)**
- BW: `lib/db_storage/` (abstraction) + `lib/db_storage_mysql/` (entity_table mappings, buffered_entity_tasks, bw_meta_data, column_type.cpp) + `lib/db_storage_xml/` (xml_database.cpp etc.) — **MySQL + XML dual backends**, consistent with 36's statement.
- KBE: `kbe/src/lib/db_interface/` (abstraction: db_interface.{h,cpp} + the entity_table.h base) + `kbe/src/lib/db_mysql/` (entity_table_mysql.* etc.) + **`kbe/src/lib/db_redis/` (a full entity-storage backend: entity_table_redis.{h,cpp,inl}, kbe_table_redis.*, db_interface_redis.*, db_transaction, redis_watcher — entity_table_redis.h:10 `#include "db_interface/entity_table.h"` implements the same abstract base)**.

**Conclusion (correcting 36 #17)**: 36's "KBE MySQL-only (directory evidence)" **is inaccurate** — KBE is likewise a pluggable abstraction + dual implementations (MySQL + Redis). It should be rewritten as "both engines are pluggable abstractions: BW = MySQL+XML, KBE = MySQL+Redis (the Redis backend is real code inside the fork; its production readiness and upstream-mainline status unverified the stated baseline — honestly marked)". The argument for apollo's "storage.xml statements-as-data leaves a natural SQL-dialect seam" is unaffected (the existence of the abstraction layer actually strengthens "dialect seams are the industry norm").

**Data structures/defaults**: per §5's entries; KBE's per-backend SQL/Redis statement mapping in entity_sqlstatement_mapping (§6).

---

## §18 Frame Sync (36 #18: keep only the adaptation seam) — negative-space confirmation

**Grade-A negative-space evidence (source baseline; search commands and scope recorded as-is)**:
```
rg -il "lockstep" ~/workspaces/BigWorld/programming/bigworld/{lib,server}   # zero hits
rg -il "lockstep" ~/workspaces/kbengine/kbe/src                              # zero hits
rg -il "lockstep" ~/workspaces/skynet/{skynet-src,service-src,lualib,service} # zero hits
```
**Conclusion**: zero lockstep implementations across the three repos — 36 #18's "no framework has built-in lockstep" holds at grade A (the Pomelo/NFG/UE sides keep 36's original grade-C annotation; no new web searches the stated baseline). The apollo-side deterministic-replay seam cites docs/25 §6.
Known defect surface (borne by apollo itself): the deterministic floats/deterministic iteration order lockstep needs have no precedent constraints in big-world engines — exactly the reason to "keep the seam, build nothing".

---

## 19. Not-Found and Unverified List (summary, preventing future mis-citation)

| Item | Status | Handling |
|---|---|---|
| An independent config default for the BW ghost trigger threshold | not located (no separate parameter item seen) | honestly marked in §9 |
| An authoritative upstream line on the skynet harbor→cluster replacement timeline | the local docs are placeholders | honestly marked in §2; the code facts suffice for the verdict |
| Production readiness/upstream-mainline status of KBE's db_redis backend | only existence and interface implementation proven | honestly marked in §17 |
| A KBE server-side script hot-reload mechanism | not detected within the range read | honestly marked in §12 (negative space) |
| Source-level implementation of UE COND_* | no UE source locally | §11 keeps the grade-C official-doc citation, no line numbers |
| A BW source counterpart of 36 #15's "segmented 64-bit" | **disproven** (EntityID=int32) | the §15 correction row |
| 36 #17's "KBE MySQL-only" | **disproven** (db_redis exists) | the §17 correction row |

---
