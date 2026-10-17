
# Apollo XML Generation and Data Loading Design (xml-generation)

> Status: design draft (under review). Positioning: answers "where XML is used in the framework, what gets generated from XML, how it is validated, how it hot-updates" — turning architecture-review §15.3/§15.4/§15.5's three XML decisions (contract source XML+XSD, split-file layout, storage statements as data) into one unified generation and loading pipeline. Cross-referenced with `sdk-contract.md` (the four projections and the gen/ directory), `attribute-sync.md` (L1 static config loading), `scripting-lua.md` (the hot-replacement protocol), `net-abstraction.md` (the frame ver→filter stack declaration), `docs/analysis/architecture-review.md` §16 (industry XML comparison).

---

## Executive Summary

1. **Status quo: apollo runtime XML consumption is zero** — the only XML reference in the whole repo is config_manager.cpp's `parseXml` **always-false stub** (:564-570, the comment admits "XML parsing needs a dedicated library (e.g. tinyxml2)", the same class as the parseLua stub); the only implemented parsers are the two hand-written ones, INI/JSON; vcpkg carries no XML library at all. XML's purpose here is not existing-code inertia but **this round setting the rules**: industry-consistent shape (BigWorld/KBEngine whole-ecosystem XML contracts, MyBatis mapper XML) + the decision already made in §15.3.
2. **Four whitelisted uses, four forbidden zones**. Whitelist: ① contract source (sdks/contract/*.xml + apollo.xsd); ② L1 numeric config tables (config/tables/*.xml + tables.xsd); ③ storage.xml storage statements (§15.5 already decided; handwritten, not generated); ④ frame-pipeline filter stack declaration (rides with the contract; net-abstraction placement). Forbidden zones: online protocols (always binary), the Lua script domain, level/geometry bulk data (binary chunks), logs.
3. **Generation source, three-way choice; "XSD pre-validated XML instance" selected**: C++ struct reflection (C++20 has no stable static reflection + truth inversion) and Lua tables as source (loses the XSD dividend + script-domain/data-domain confusion) are both rejected — comparison table in §3.
4. **Five artifact classes + the v3 contract reflection backend**: strongly typed data structures, typed loaders (pugixml; errors carry file:line+xpath), startup validators, the schema_hash header, doc projections — the contract source additionally appends, in the same batch per sdk-contract §10.3 v3 (2026-09-29), five reflection-backend artifacts (.proto/descriptor.bin/contract.lua/semantic.json/contract_route, P2, see §4); storage.xml is handwritten, not generated.
5. **Validation, the four-layer funnel**: xmllint XSD gate (CI) → generator semantic rules (what XSD cannot express: ID segmentation/cycle detection/cross-table references) → generated-code compile time (static_assert/constexpr) → startup fail-fast (aggregated error reporting — report everything at once, not stop-on-first-error). Principle as in §0: what can be reported at generation time does not wait for compile time; what can be reported at compile time does not wait for startup; what can be reported at startup does not wait for runtime.
6. **What to take and what to drop from the MyBatis paradigm**: take "XML description + declarative statements/mappings + full parse at startup failing fast + rebuild-style hot update"; drop "everything of runtime ORM" (sessions/lazy loading/L2 cache/dynamic SQL/`${}` concatenation). Game-server mappings and the non-fit list are in §6.

---

## 1. Status-Quo Inventory (Code-Reading Conclusions)

### 1.1 Census of XML Consumption in apollo Code (this round's grep, actually run)

| Fact | Location | Verdict |
|---|---|---|
| The repo's only XML library reference (tinyxml/pugixml/rapidxml/libxml/expat/xerces) | `modules/core/config/src/config_manager.cpp` | This one spot only |
| `parseXml` always-false stub | config_manager.cpp:564-570 | "Declared but not implemented" — a member of the C-49 stub family (same batch as the parseLua stub :572+) |
| Implemented parsers: hand-written INI/JSON | config_manager.cpp:412/:493 | The de-facto main config format is JSON (vcpkg's only data-format dependency is nlohmann-json) |
| `ConfigFormat::Xml` routing and .xml extension dispatch already exist | config_manager.cpp:115-116/:131-132/:632-633 | The seam stays unimplemented — the routing is deleted together with the stub (architecture-review §16.8.3-③): XML loading goes only through this design's §4 artifact 2, the generated typed loader, never back-feeding the config_manager generic tree |
| Client SDK (sdks/skds) XML usage | No hits | The client is likewise zero-XML |

### 1.2 Industry Comparison (extension of architecture-review §16 source evidence)

| Framework | XML usage surface | Where XML is not used |
|---|---|---|
| BigWorld | Entity .def (XMLSection/BWResource, entity_description.cpp:184-190), the resource and config ecosystem | Online protocols (the Mercury binary Bundle), space/geometry data (binary chunks) |
| KBEngine | entities.xml + entity_defs/*.def (tinyxml2, entitydef.cpp:188-210), the server config kbengine_defaults.xml | Binary protocols, binary navmesh |
| skynet | **Zero XML** (the counter-example: sproto schema + Lua config) — its versionless, validation-less schema is exactly the path §15.3 rejected | Everything |
| MyBatis | mapper XML (statements/mappings made declarative, §6 in detail) | The runtime data itself (it lives in the DB) |

Conclusion: **"XML for contracts/config, binary for hot-path data" is the mainstream shape of MMO servers**; apollo carries zero legacy baggage, so the rules are set directly in this shape at no migration cost.

## 2. Delimiting XML Usage: Whitelist and Forbidden Zones

| # | Use | Files | Who writes | Who reads | Change frequency | Validation |
|---|---|---|---|---|---|---|
| ① | Contract source | `sdks/contract/{apollo.xsd, attrs.xml, messages.xml, entities.xml, errors.xml, version}` (§15.4 layout) | Programmers (messages/errors) + designers (attrs/entities) | Generator + xmllint | attrs high-frequency, messages low-frequency | apollo.xsd + generator rules |
| ② | L1 numeric config tables | `config/tables/*.xml + tables.xsd` (leveling curves/monster parameters/buff values etc., the L1 of attribute-sync §2.1) | Designer-tool export (Excel/CSV → exporter → XML; the exporter belongs to the toolchain, outside this design; the only interface is XML+XSD) | Server startup loader | High-frequency (hot update) | tables.xsd + startup validation |
| ③ | Storage statements | The server-private `storage.xml` (tables/column promotion/journal statements, §15.5) | Programmers | DB executor | Low-frequency | storage.xsd (keyref locked back to contract attr ids) |
| ④ | Filter stack declaration | The channel/version fields riding with the contract messages.xml | Programmers | Generator → L1 assembly code (net-abstraction §3 placement) | Low-frequency | apollo.xsd |

**Forbidden zones (written into review red lines)**:
- **Online protocols are always binary** — XML is only the contract source and config source; generator artifacts are what goes on the wire (the §15.3 unchanged item).
- **The Lua script domain takes no XML** — formulas/behavior are code (the scripting-lua boundary); config tables carry numbers only.
- **Level/geometry bulk data takes no XML** — binary chunks + indexes (industry-consistent); XML stores only reference metadata.
- **Log/BI data takes no XML** — structured text/columnar collection (attribute-sync §8.2 BI triage).

## 3. Generation-Source Decision: Three-Way Choice

| Candidate | Assessment | Verdict |
|---|---|---|
| **XSD pre-validated XML instance** | Language-neutral (one source for the C++/C#/TS three ends); the four XSD dividends ready-made (§15.3: key uniqueness/keyref references/enumeration/zero validator development); designer-readable and diffable; isomorphic with BigWorld/KBEngine .def (the lineage after the C-50 correction) | **Selected** |
| C++ struct reflection | C++20 has no stable static reflection (lands only in 26); truth inversion — code becomes the source, the contract degenerates into a generated artifact, and multi-end projection loses its language-neutral source; changing one attribute name means touching headers + recompiling every end | Rejected |
| Lua tables as source | Loses the XSD dividend (Lua has no schema layer); script-domain/data-domain confusion (the scripting-lua §1 boundary: attribute-value storage never enters Lua); designers hand-editing Lua tables = no validation, straight to production | Rejected |

Boundary note: **the generator does not consume the XSD itself** — the XSD defines structural rules (for validation); the generator's type/semantic input is the XML instances + the per-file-class generation rules; XSD changes (adding an enum value) require syncing the generator rules, and their mutual consistency is backstopped by the layer-② generator assertions (§5).

## 4. Generator: Input / Artifacts / Implementation

- **Implementation form**: one C++ binary `sdks/gen/` (sdk-contract §3 already decided; enters CI); XML parsing via **pugixml** (one new vcpkg entry, §15.3 already decided); **XSD validation is not done inside the generator** — pugixml has no XSD capability and libxml2 is not worth introducing for it; the CI gate fronts `xmllint --schema` (§15.3 "zero validator development"); **include aggregation is self-implemented in the reading layer** (sdk-contract §11.4: pugixml has no built-in XInclude; reading the root file recursively expands references into a single document tree; XSD validation and schema_hash are always taken against the aggregated whole — zero new dependencies). Linking boundary (architecture-review §16.8.3-②): gen links zero runtime code — the artifacts are linked by consuming modules; the generator itself enters no runtime target (sdk-contract §3 syncs this boundary).
- **Contract reflection backend (v3, sdk-contract §10.3/§10.6 decided, P2)**: the contract source appends, in the same batch, five reflection artifacts — `.proto` (golden, the source for protoc) → protoc emits `descriptor.bin`; `contract.lua` (a Lua table dump of the IR: attr tables/message routing/whitelists; require-and-use on the server); `semantic.json` (a small client semantics piece); `contract_route` (the C++ frame-routing manifest: msg id→name/dir/domain/handler); client-side code wrapping (protoc `--csharp_out`/pbjs; CI single point emits golden into the repo). Generation semantics per `binding="native|reflect"` (the sdk-contract §10.6 v3 message dichotomy): native (framework-fixed message families) goes into built-in strongly typed code; reflect goes only into bin/routing. Implementation approach: **no homegrown AST** — the pugixml DOM is the parse tree (a direct dividend of choosing XML); the core assets are the IR (ContractModel) + the four-layer funnel, and every artifact is a thin "read IR → render text" writer; business-message `.pb.cc` is not produced by default. **First batch landed (2026-09-29, apollo-gen 0.2.0 + contract v2)**: messages `domain` (required)/`binding` (default per channel) attributes, domain segmentation (client 1-899 / internal 900+, segment values fixed from then on) and the cross-domain reference ban enter parser layer ②; the `contract.lua`/`semantic.json`/`contract_route.json` trio ship in the same batch as .h/.json and enter the golden gate (the Lua handler naming convention = the message name). **The `.proto` dual projection landed as the second batch the same day**: `apollo_contract.proto` (full set, the server bin's source) + `apollo_contract_client.proto` (the client-domain projection, the client bin's source) — the §10.2.2 type mapping (sint32/sint64 zigzag, alias zero special-casing, lists repeated), the MsgId enum (MSG_ prefix — protobuf enum values live in the package-level sibling scope), field numbers 1..N in declaration order; local protoc `--descriptor_set_out` verification passed (descriptor.bin is still finalized by the CI single point per discipline, never entering developer environments). **The dual-domain hash landed as the third batch**: `computeDomainHash` (the algorithm of §6, input filtered by domain — the client face = client-domain messages + attrs + errors, the internal face = internal-domain messages + entities; **the hand-written version does not enter the domain bundle**, otherwise internal-only changes would bump client_hash too); artifact carrying is tiered — the five server artifacts carry schema_hash + the dual-domain hash, **client pack members (semantic.json/client proto) carry client_hash only**, dropping schema_hash and contract_version (both are bumped by internal-only changes, which would break the byte-invariance of the files inside the `dist/client/<client_hash>/` directory, §11.6); after an internal-only change + version bump, the client pack members stay byte-identical while all server artifacts change — verified locally. Client code wrapping and the load-time consistency gate follow in later batches.
- **Artifacts (five classes)**:
  1. **Strongly typed data structures**: contract → `enum class AttrId`/`enum class MsgId` + constexpr default-value tables (the machine-readable contract form of attribute-sync §2); tables → one struct per table (field types calibrated to the tables.xsd type set: int64 ten-thousandths/strings/arrays).
  2. **typed loader**: generated loading functions from pugixml DOM → struct; errors carry `file:line + xpath + expected/actual` (pugixml's node.offset_debug yields the line number) — generic ConfigNode-tree second-pass reflection is banned (hot-path type safety is fixed at compile time).
  3. **Startup validator**: keyref/ranges/cross-table references/derived-DAG topology (the cycle-detection placement of 16.7.2); **aggregated error reporting** — collect all errors, output once (designer-tool friendly), not stop-on-first-error.
  4. **schema_hash header**: SHA-256(contract sources + generator version) (the sdk-contract §6 handshake unchanged).
  5. **Doc projection**: protocol docs/error-code tables (sdk-contract §3 artifact ④).
- **Explicitly not generated**: storage.xml (handwritten + storage.xsd validation, §15.5 — statements are programmer knowledge, not a generation target); **its structural evolution surface is likewise handwritten in the same domain** — DDL versioned migration (db/migrations + the position table + forward-only + destructive statements banned) see attribute-sync §8.3; the generator only emits the "contract column:true disagrees with the DB structure" CI diff reminder and produces no migrations; C#/TS SDK projection details (sdk-contract §10.3 v3, the same generator, later scheduling — protoc/pbjs ready-made tools emit the code; not repeated here).
- **CI dual gate** (sdk-contract §3 already decided; the implementation approach given here): gate one `xmllint --schema` over all XML; gate two the generated-artifact diff check (edit the contract, forget to regenerate → red). The generator's self-check rules are exactly §5 layer ②. Both gates wired (2026-09-29): gate one = the `xsd_gate` job of workflow `contract.yml` (any *.xml in the directory containing include fragments always passes apollo.xsd — a fragment is itself a complete root element, independently validatable); gate two = ctest `apollo_gen_golden_check` (seven artifacts byte-diff, run with the ci.yml matrix).
- **include aggregation landed (§11.4, the fifth batch the same day)**: `expandIncludes` into the directory-level reading layer — recursively expands the root's direct-child `<include href>` into a single document tree (depth-first; cycles/missing files/root-element mismatch/version mismatch/missing href/unknown attributes/carried child elements each have a dedicated diagnostic; deep includes are not expanded — the strictWalk whitelist blocks them; duplicate includes may concatenate, duplicate keys are caught by the existing rules). The four XSD roots gain the include declaration (position locked to the head of the root's children; main child elements relaxed to minOccurs=0 — a pure-include root is legal); **the model and schema_hash parsed from a split-file directory and from a single-file directory are exactly equal** (locked by test — physical file splitting does not affect hash stability). Single-file parseXxxXml does no expansion (no filesystem context; include is a directory-level reading-layer capability).

## 5. Validation Strategy: The Four-Layer Funnel

| Layer | Timing | Tool | What it catches |
|---|---|---|---|
| ① XSD gate | CI / editor realtime | xmllint --schema (free IDE completion) | Structural validity: xs:key (attr/msg id uniqueness — C-35 segmentation conflicts cannot enter the repo), xs:keyref (derived-DAG dangling references, storage column-promotion references, the parent references of 16.7.2), xs:enumeration (misspelled domain/channel/ownership) |
| ② Generator rules | CI (the first half of gate two) | Generator assertions | **The semantics XSD cannot express**: ID-segment non-overlap, derived-DAG cycle detection (neither framework's .def parser does it; 16.7.2 proves it must be self-written), cross-table reference existence, same-name type-change rejection (the Avatar←Monster provenance chain), the version consistency of XSD vs generator rules |
| ③ Compile time | Build | static_assert / constexpr | Enum boundaries, default-value table completeness (a constexpr array indexed by enum — a missing item is a compile error), the schema_hash constant in sync with the header |
| ④ Startup fail-fast | Server startup | Startup validator | Data-row ranges/unit calibration, cross-table references, contract-config version match; refuse to start after **aggregated error reporting** |

Order is principle: every level an error sinks doubles the cost (a layer-② generation error is fixed by reading the printout; a layer-④ startup failure waits for the deployment site).

## 6. MyBatis Reference Research: Mapping and Non-Fit in the Game-Server Context

### 6.1 The MyBatis Paradigm at a Glance

The mapper XML three-piece set: **declarative statements** (namespace + statement id + SQL, `#{}` precompiled parameters vs `${}` string concatenation), **declarative resultMap mapping** (column↔field bound by name + type-conversion declarations; association/collection handle relations), **interface binding** (namespace = the Mapper interface's fully qualified name, statement id = the method name; runtime dynamic proxying to the statement). Framework behavior: SqlSessionFactory **fully parses** the mapper XML at build time — duplicate statement ids, bad statements, missing references **fail at startup**; the L2 cache `<cache/>` is namespace-granular and off by default; the official position **does not support runtime mapper hot-swap** (the only option is rebuilding the SqlSessionFactory wholesale).

### 6.2 Game-Server Mapping (Alignment Points)

| MyBatis concept | apollo placement | Status |
|---|---|---|
| Statements as data (mapper XML) | storage.xml mapped statements (§15.5 already decided: three write-behind statements + column-promotion reads/writes, bound by name, validated at startup) | Adopted |
| Full parse at startup, fail-fast | Layers ②/④ of the four-layer funnel (generator assertions + startup validation fail-fast) — the same philosophy of "errors reportable at startup do not wait for runtime" | Adopted |
| Declarative resultMap column↔field mapping | **Config table loading**: tables.xsd declares columns (field name/type/default); the generator produces row↔struct name-bound loading code — type conversion lives at the declaration layer (ten-thousandth calibration/enum string → enum class); no runtime reflection guessing | Adopted by this design (§4 artifact 2) |
| Rebuild-style hot update (rebuild SqlSessionFactory) | **The config-table hot-update pipeline**: FileWatcher/GM trigger → a worker thread fully re-parses + validates through the full funnel → atomic table swap at the tick boundary → on failure keep the old table + alarm (isomorphic with the scripting-lua §3.2 table-swap protocol; §3.3 version partial-order interlock). Where the analogy holds: **not incremental mutation of table objects, but wholesale rebuild then swap** — MyBatis officially offers rebuild as the only path, which is precisely the shape a game server wants | Adopted by this design |
| `#{}` precompiled vs `${}` concatenation | The only binding path (name binding + parameterization) — the C-44 field-tested defect cluster (no escaping/conditional escaping/silent defaults) structurally vanishes under "one path only" (§15.5 already argued) | Adopted |

### 6.3 Non-Fit List (a Game Server Is Not an ORM Context)

| MyBatis mechanism | Reason for not introducing |
|---|---|
| SqlSession/L1 cache/identity map | The in-memory authoritative model (attribute-sync §3): in-process attributes are the truth; no session-level identity mapping is needed |
| Lazy loading / N+1 | Table loading is a one-time full load at startup + a full rebuild on hot update; no per-row lazy-fetch scenario exists |
| L2 cache `<cache/>` | Breaks the single-writer and write-behind discipline (§15.5 already rejected; the Redis layer likewise, attribute-sync §8.2) |
| Dynamic SQL (the `<if>/<foreach>` runtime interpreter) | The statement surface is small and stable (§15.5 already rejected): a mini-language interpreter is a liability |
| `${}` string concatenation | An injection surface, a cautionary example — the only SQL generation point on the apollo side (the storage statement parameters) is forcibly parameterized |
| Mapper dynamic proxy | C++ has neither the mechanism nor the need for it: generation time directly produces strongly typed functions (the interface-binding idea retained — "find the statement by name" collapses into "the function generated by name"; a wrong name goes red at compile time) |

## 7. Phasing

- **P1**: contract generator v1 (contract/*.xml → artifacts 1-4 + the doc projection; the CI dual gate xmllint+diff online) + tables loader v1 (tables.xsd + the generated loader + startup validation) + the **honest deletion** of config_manager's parseXml/parseLua stubs and the ConfigFormat::Xml/Lua routing (stub-family cleanup, C-49 discipline; architecture-review §16.8.3-③: the deletion includes the routing itself, and no generic XML implementation is added — config_manager keeps its INI/JSON generic-tree duty; XML loading goes only through this design's artifact 2; the change points are recorded on file, executed in the code phase).
- **P2**: the config-table hot-update pipeline (FileWatcher → full rebuild → tick-boundary table swap) + the **contract reflection backend** (sdk-contract §10.3 v3: the five artifacts .proto/bin/contract.lua/semantic.json/contract_route + the binding generation semantics + include aggregation expansion; **the first three batches delivered** — domain/binding + domain-segmentation validation + contract.lua/semantic.json/contract_route.json + the .proto dual projection + the dual-domain hash (client_hash/internal_hash), see the §4 landing notes) + the doc projection onto the sdk-contract schedule (client code wrapping) + frame-pipeline filter stack generation (interlocked with the net-abstraction P1 session layer).
- **P3**: storage projection tooling (the storage.xml startup validator reuses the four-layer funnel) + a binary-packaging option for size-sensitive tables (generation-time packaging + schema version; zero XML parsing at runtime).

## 8. Intersections with Other Designs

| Related design | Placement |
|---|---|
| sdk-contract.md | This design = the internal design of its gen/ generator (same binary, same CI dual gate); the four projections and the directory layout, the schema_hash handshake — all reused, not redefined |
| attribute-sync.md | §2.1 L1 static-config loading and hot update are exactly this design's use ②; the contract attrs.xml is the core instance of use ① |
| scripting-lua.md | Hot updates go through the same table-swap protocol (§3.2's three phases); the version partial order of config hot update vs script hot update (§3.3); the "data does not enter scripts" boundary is the same line as §3's rejection of Lua tables as source |
| net-abstraction.md | The frame-header ver field → the filter stack declaration (16.7.1); the contract channels/enumeration validated by §5 layer ① |
| ssengine-reference.md | utils/config/FileWatcher (inotify/poll) as the ported hot-update trigger; the "header comments lie" lesson ↔ the honest-izing of the config_manager stubs |
| architecture-review.md | The §15.3/15.4/15.5 decision source; §16 industry XML comparison (after the C-50 .def=XML correction, this design is isomorphic with both precedents); the C-49 stub family → the §7 P1 stub-cleanup items |

---
