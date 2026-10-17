
# Apollo Multi-Client SDK Contract Design (sdk-contract)

> Status: Contract sources, XSD validation, generator, generated artifacts, and golden checks are implemented in the repository. Multi-client runtime integration, protocol negotiation, and compatibility-window rollout are not yet complete. Planned items below describe target work, not implemented behavior. Reference: the essence of KBEngine's "standalone `sdks/` directory + contract-defined protocol conventions" (learn the essence, not the form), with its shortcoming corrected (its data storage is too weak — analysis in `docs/design/attribute-sync.md` §8). This design cross-references `attribute-sync.md` (attribute model/prediction/attr_schema_hash), `net-abstraction.md` (frames/channels), `scripting-lua.md` (script whitelist), and `architecture-review.md` (assembly discipline).

---

## Executive Summary

1. **The contract files are the single source of truth**: `sdks/contract/`'s `attrs.xml` + `messages.xml` (XML + XSD, architecture-review §15.3/§15.4) define all attributes and messages (fields/types/visibility/sync groups/predict bits/channels); the **code generator** produces the server-side accessors + each client SDK's serialization and client attribute model + documentation (the generator's internal design is in `xml-generation.md`). At M0 the reality was the reverse: `modules/protocol` hand-written encoding, the whole repo (SDK included) with no .proto/.def/.toml contract whatsoever, clients hand-writing `MessageCodec.ts`/`ByteBuffer.ts` — the protocol maintained once per client, bound to drift. The single contract source of truth has been delivered with P2/P3 (sdks/contract XML+XSD + apollo_gen + golden .proto); the handwritten codec is the remaining gap.
2. **The directories converge to a single `sdks/`** (the typo'd directory `skds/` merges back under the correct name). Facts: `git log 38656f90 refactor: 重命名 sdk 目录为 skds` — a rename introduced a typo, after which new work (the unity cocos laya Network/Session/Messaging layers) landed in `skds/`, while the old `sdks/` retained another set of content (unity's Attributes system), **two mutually exclusive sets that don't talk to each other**. The structure aligns with KBEngine: `sdks/{contract,gen,unity,ue5,cocos,laya,cpp,docs}`.
3. **The one thing we learn from KBEngine: contract-driven multi-client.** But three things we do not copy: (a) we do not replicate .def's raw XML parsing — **C-50 correction: .def is XML to begin with** (BigWorld entity_description.cpp:184-190, KBEngine entitydef.cpp:188-210, architecture-review §16.3); the real difference is that both families **only parse, with no schema layer** (misspelled tags silently default); apollo uses XML + XSD formal validation (architecture-review §15.3) — designer-readable, diff-friendly, zero validator development; (b) no "one generic serialization for every client" — the generator tailors the shape per client (C# events, TS modules, C++ accessors are each one's own idiom); (c) **storage does not enter the contract** — KBEngine's .def couples attribute shape with storage (blob-ifying complex types, base-only snapshots; see attribute-sync §8's five points); the contract governs only the wire protocol shape.
4. **Client SDK ≠ serialization layer**: four layers of responsibility (Codec / Attribute client model / Entity·AOI management / event and presentation routing), where layer 2 lands attribute-sync §9's prediction matrix (predicted/authoritative double buffering + correction) and layer 3 lands incremental application of enter/dwell/leave snapshots. The current `sdks/unity/ApolloSDK/Attributes/` AttributeContainer/AttributeSyncManager is layer 2's prototype, and `skds/`'s MessageCodec/MessageRouter is layer 1's — after the merge, reorganize along these four layers.
5. **Compatibility is hash-driven**: `attr_schema_hash` = the generator's hash over the contract content (not a hand-maintained version number, guarding against missed updates), verified by handshake the moment a connection is established (attribute-sync §7.2); the server simultaneously supports the two hashes N/N-1; change discipline: adding fields is compatible, changing semantics bumps the version, deletion requires both ends in the same cycle.
6. **Storage evolves freely**: persistent columns/journal replay (attribute-sync §8's countermeasures) evolve independently inside the server, unconstrained by contract freezes — adding a persistence field doesn't touch the contract, and vice versa. This is the dividing line where "storage is not weak" and "the protocol is clear" both hold.

---

## Protocol Updates and Code-Only Updates

Apollo defines two release modes. A **protocol update** changes contract XML/XSD and regenerates artifacts through `apollo_gen`; servers and clients must handle old and new contracts during a compatibility window. Contract sources, the generator, and generated-artifact checks exist, but N/N-1 negotiation, compatibility-window checks, and rolling-release validation are incomplete. Protocol changes therefore are not yet a proven no-downtime hot update.

A **code-only update** keeps the contract and wire format unchanged. Lua `ScriptRuntime` provides preparation, tick-boundary commit, observation, and rollback primitives. Production control-plane delivery to every World remains incomplete. C++ changes use process replacement after instances drain, or after a separately implemented instance-state transfer; C++ cannot be loaded into a running process.

## 1. Current State and Problems (code-reading conclusions)

| Fact | Location | Problem |
|---|---|---|
| Two SDK directories coexist with mutually exclusive content | `sdks/` (only unity/ApolloSDK/Attributes/*.cs: AttributeDefinition/Registry/Container/SyncManager/ComputedAttribute) vs `skds/` (README + unity/cocos/laya: ApolloClient/Config, Network, Session, Messaging, Utilities) | after the same rename accident (git 38656f90 `重命名 sdk 目录为 skds` introduced the typo), both sides evolved separately; `skds/unity` has no Attributes layer and `sdks/unity` has no Client/Network layer — no client can assemble a complete SDK |
| No single source of truth for the protocol | `modules/protocol/src/messages.cpp`, `codec.cpp` (handwritten); `skds/*/Messaging/MessageCodec.ts`, `Utilities/ByteBuffer.ts` (handwritten per client) | the server and each client each maintain their own encoding; a field add/remove/change must be synced in N places, and one miss misaligns the wire; no .proto/.def/contract file of any kind |
| Attribute definitions duplicated | server `include/apollo/game/attributes/attribute_id.h` (~300 legacy IDs) + client `AttributeRegistry.cs` | the same attribute table handwritten in two places; the segment-overlap defect (attribute-sync §0) reproduces on the client verbatim |
| The directory name "skds" is a typo | provable via `git log` | generators/docs/URLs all have to follow the wrong name |

The post-merge target structure (§3) eradicates these problems in one stroke: the contract is produced, the generator collects it, and no client ever handwrites protocol again.

## 2. Contract Shape (learn the essence, not the form)

### 2.1 What We Learn

KBEngine: `.def` files declare entity attributes and methods → the generator simultaneously produces server C++ and multi-client code — **declared once, implemented on every client, aligned at compile/startup time**. This solves the entire pain of MMO multi-client protocol drift; it is the essence, and this one thing is all apollo adopts wholesale.

### 2.2 What We Change

| KBEngine .def | apollo contract | Reason |
|---|---|---|
| Raw XML parsing, no schema layer (C-50: .def is XML to begin with; citations in executive summary 3(a)) | XML + XSD (`attrs.xml` / `messages.xml`, compact one-attribute-per-line style, see §2.3) | what we learn is the two families' isomorphic lineage; apollo's increment = the XSD formal validation layer (neither family has it) |
| Attributes/methods/storage coupled in one file | the contract governs only the **wire protocol shape**; the storage model stays inside the server (§7) | KBEngine's number-one shortcoming (attribute-sync §8) |
| One generic client-code generation | per-client tailored generation (C#/TS/C++ each in its own idiom) | client differences are not serialization differences; they are event and object-model differences |
| No version/hash | files carry `version`; the generator produces `schema_hash` | the data basis of the compatibility policy (§6) |

### 2.3 Contract Files and the Compact Style (XML + XSD, revised in sync 2026-09-28)

```xml
<!-- attrs.xml —— entity attributes (validated by apollo.xsd; aligned with attribute-sync §2's synced-attribute convergence) -->
<attrs version="7">
  <attr id="1" name="hp" type="int64" init="1000" scope="BASE" sync="attributes"
        persist="false" predict="client_predicted,server_authoritative"/>
  <!-- type:  int64/fixed bit width/int64 basis points/string/string[]/slice containers
       scope: BASE/CELL/RO_MIRROR × SELF/TEAM/GUILD/AOI/WORLD (xs:enumeration, attribute-sync §2.3)
       sync:  channel name movement/attributes/events/control (net-abstraction §4.1)
       persist/column: directional hints for two-stage storage (the real definition lives in the server's storage.xml, §7) -->
</attrs>

<!-- messages.xml —— messages/intents (direction + channel + payload) -->
<messages version="7">
  <msg id="10" name="move" dir="C2S" channel="movement">   <!-- intent only, the server decides -->
    <field name="x" type="f32"/><field name="y" type="f32"/>
    <field name="z" type="f32"/><field name="yaw" type="f32"/>
  </msg>
  <msg id="11" name="attr_batch" dir="S2C" channel="attributes">
    <field name="seq" type="u32"/><field name="base_seq" type="u32"/>
    <field name="deltas" type="AttrDelta[]"/>
  </msg>
</messages>

<!-- entities.xml —— entity inventory and inheritance (architecture-review §16.7.2: single inheritance, expanded at generation time; xs:keyref locks parent references, cycle detection in the generator).
     <attr name="…"/> child elements declare the entity's own attribute set (G-6 expansion batch): expanded set = own ∪ ancestor-chain
     union; a child re-declaring an attribute an ancestor already carries errors at the parsing layer. Attribute names referencing across
     files is beyond single-document XSD validation — dangling/duplicate adjudication and cycle detection both live in the parser's semantic layer. -->
<entities version="7">
  <entity id="Monster">
    <attr name="hp"/>
    <attr name="level"/>
  </entity>
  <entity id="Avatar" parent="Monster">
    <attr name="gold"/>
  </entity>
</entities>
```

Style discipline (§15.3): compact one-line-per-attribute elements, no child-element nesting; `xs:key` locks id uniqueness (C-35 segment conflicts cannot enter the repo), `xs:keyref` locks references (derivation DAG/parent/column promotion), `xs:enumeration` locks domain/channel/ownership values.

## 3. Directory Structure and Generator

```
sdks/
├── contract/        # contract sources (single source of truth): apollo.xsd, attrs.xml, messages.xml, entities.xml, errors.xml, version (architecture-review §15.4 layout)
├── gen/             # generator (a single C++ binary, in CI; failure blocks protocol release)
├── cpp/             # generated artifacts + handwritten shell → linked into modules/protocol as a module
├── unity/           # U3D plugin (generated + handwritten runtime shell)
├── ue5/             # UE5 plugin (planned P3)
├── cocos/           # Cocos Creator TS (generated + handwritten shell)
├── laya/            # LayaBox TS (shares the generation core with cocos)
└── docs/            # protocol docs/changelogs/error-code tables (generator artifacts)
```

Generator input = contract files; output = five classes of artifacts: (1) C++ encoding/accessors and validation (server + the cpp SDK), (2) C# (Unity), (3) TS (Cocos/Laya, one template instantiated twice), (4) protocol documentation, (5) the `schema_hash` constant header. **CI-enforced**: a contract change must commit the regenerated artifacts together (pip-style diff check), preventing "changed the contract, forgot to generate".

Linking boundary (architecture-review §16.8.3-②): gen is a build-time tool **outside the runtime dependency graph** — linked by no runtime target; its artifacts are consumed by modules/protocol, the tables-consuming modules, and each sdks/*; runtime code is forbidden from including gen's internal headers. Precedents: KBEngine's config converter lives outside the source tree (kbe/tools/xlsx2py); the BigWorld tool family are standalone binaries (server/tools).

## 4. The Four Layers of Each Client SDK's Responsibility

| Layer | Generated/handwritten | Responsibility |
|---|---|---|
| 1. Codec | generated (frame-delimiting template + message table) | frame header/CRC/encode-decode (net-abstraction §3 L1); the server and every client share the same table in the same order |
| 2. Attribute client model | generated + handwritten shell | visible-attribute container → dirty subscription → **predicted/authoritative double-buffered mixer** (attribute-sync §9's landing: movement prediction + correction; HP interpolation presentation-layer only; everything else replaced immediately with the authoritative value) |
| 3. Entity/AOI management | generated + handwritten shell | enter-view full snapshot → dwell incremental application → leave cleanup (attribute-sync §6); entity id → on-device object mapping |
| 4. Event and presentation routing | handwritten | tween/animation state machine/UI binding interfaces; device-specific, unconstrained by the contract |

Current-state mapping: `sdks/unity/ApolloSDK/Attributes/{AttributeContainer,AttributeSyncManager,AttributeEvents,ComputedAttribute}.cs` ≈ layer 2's prototype (kept, fixed, and merged in); `skds/*/Messaging/{MessageCodec,MessageRouter}.ts`, `Utilities/ByteBuffer.ts` ≈ layer 1's prototype (rewritten as generated artifacts); the rest (AuthManager/HeartbeatManager) stays as handwritten shell. **After the four-layer reorganization each client maintains only its handwritten shell layer; the protocol layer always follows the contract.**

## 5. Server Integration

- The generated artifacts of `modules/protocol` replace the handwritten `codec.cpp/messages.cpp` (net-abstraction §1's verdict); whether `nng_wrapper` is kept for the inter-process path only follows the net-abstraction §6 decision.
- Attribute accessors: contract attr id ↔ server-runtime AttributeDef generated from **the same L1 static configuration** (attribute-sync §2.1's L1 takes the contract files as its source); the server attribute table and the client attribute table are constructively co-sourced, and `attribute_id.h`'s legacy hand-written ID table retires with this.
- Handshake: on connection establishment, exchange `schema_hash`; the server supports the two hashes N/N-1 (§6); on mismatch, refuse and return a rejection frame carrying the server version, and the client prompts the user to upgrade.
- The uplink intent whitelist: the script-writable whitelist of `scripting-lua.md` §5.2 **is generated from the contract** (the `predict` bit ∩ server configuration) — preventing the scripting layer and the contract from splitting again.
- The server's own deserialization (modules/tools) also goes through the same generated accessors — modules inside the server are naturally protocol-consistent.

## 6. Version and Compatibility Policy

```
contract commit → CI generation → schema_hash = SHA-256(contract sources + generator version) → each client SDK release
live servers: server_supported = {hash_N, hash_{N-1}}
```

- **Hash, not a hand-maintained version number**: a forgotten version bump → the hash changes automatically, and the handshake exposes it immediately; the version field is for humans only.
- Change discipline (written into the generator README): adding a field = backward compatible (old clients ignore the default; the server fills the default); changing type/semantics = bump the contract version (old and new hashes coexist for one cycle; remove the old hash after gradual rollout completes); deleting a field = both ends must be in the same cycle, and requires one "announcement version before deletion".
- Linkage with attribute-sync §3's switchover: snapshots carry `attr_schema_version` (scripting-lua §3.3) — while old- and new-version entities coexist in the same scene, the client picks the decode table by the entity's version number.

## 7. Decoupling from Storage (why the contract doesn't freeze storage)

KBEngine's structural weakness (attribute-sync §8, key points): .def couples attribute shape with persistence → complex types get blob-ified, persistence only does base snapshots, no journal, no query surface. The countermeasure is fixed here as one principle:

> The contract answers "how it travels the wire"; storage answers "how the server persists it" — the two meet only in `persist` as a directional hint, never bound. Derived attributes/persistent columns/journal replay are all server-internal evolution objects.

Concretely: adding "an attribute that needs archiving" changes only the server persistence layer (column promotion, attribute-sync §8.2), zero contract changes — because the client doesn't need to know which table the server stores it in; likewise, adding a contract field (e.g. a UI presentation hint) doesn't touch storage. The concrete shape of storage definitions = the server-private `storage.xml` (tables/column promotion/journal statements, a MyBatis-style "statements as data" form, xml-generation.md §6; not in the contract directory, not in schema_hash — architecture-review §15.4/§15.5 two-stage). **The protocol's and storage's evolution cycles decouple from each other — the final link of "learning the essence".**

## 8. Phasing

- **P1**: directory convergence (`skds/` content merges into `sdks/`, the empty shell deleted, the correct spelling kept; recorded via git mv); contract v1 (`apollo.xsd` + attrs/messages/entities/errors.xml, §15.4 layout); generator v1 (C++ + docs + hash, internal design in `xml-generation.md`); the Unity client reorganized along the four layers (Codec generated + the Attributes layer merged into the generated artifacts).
- **P2**: Cocos/Laya TS generation; layer 3 (AOI entity management) generated shell; prediction mixer v1 (movement double buffering); **the reflection channel and client-side code wrapping** (§10.6 v3 final decision: .proto golden → protoc in the same batch emits `descriptor.bin` (business-message reflection + the sol2 bridge into Lua) + `contract.lua` (server semantics) + `semantic.json` (client semantics) + protoc/pbjs client code (clients with code hot-update pipelines) — **reflection by default, code as the enhancement on both ends**, implemented in the same batch); **internal/external contract domain separation** (§11: the domain attribute + msg id segmented by domain + dual hash + per-domain filtering + the cross-domain prohibition, the same mechanism as the bin projection).
- **P3**: the UE5 plugin; cross-client event-protocol review; the contract-change pipeline connects to CI release (§12: the `compat_window_check` job + the VSCode plugin layer + precompiled single-binary distribution).

## 9. Intersections with the Other Designs

| Related design | Landing point |
|---|---|
| attribute-sync.md | contract §2.3 is the machine-readable form of its §2 attribute model/§9 prediction matrix; the `attr_schema_hash`/`attr_schema_version` handshake and switchover; the snapshot/delta messages are exactly `attr_batch` |
| net-abstraction.md | the frame format and four channels written into the contract (each client plugin implements frame delimiting and channel mapping from it); `modules/protocol`'s handwritten code retires |
| scripting-lua.md | the script whitelist generated from the contract (§5); the contract version delivered with entities (scripting-lua §3.3) |
| ssengine-reference.md | the source of the weak-storage lesson (attribute-sync §8); the sdpkg frame-header direction aligned with net-abstraction §3 |
| architecture-review.md | unrelated to container semantics; the extension of the "configuration exists exactly once" discipline onto the cross-client dimension; §15.3/15.4/15.5 the contract-decision source, C-50 .def correction (§16.3) |
| xml-generation.md | the gen/ generator's internal design (pugixml parsing, the four-layer validation funnel, five artifact classes, MyBatis mapping); this design's gen/ is its implementation carrier |

---

## 10. Contract and protobuf Encoding: the Two-Layer Model (Target Design)

> Status: this section describes the target encoding design. The repository has contract sources, the generator, generated artifacts, and golden checks; client runtime integration and protocol compatibility rollout remain incomplete.

### 10.0 Why the def Contract Does Not Exclude protobuf — It Depends on It

First separate three concepts often conflated:

| Concept | Essence | Question it answers | Example |
|---|---|---|---|
| def contract (attrs/messages/entities.xml + XSD) | **semantic-layer** declaration | should it be sent, to whom, when, who may write, what the value means | `hp`: int64, PROP-visible, default 1000 |
| protobuf | **wire-layer** encoding format | how values become bytes | varint strips leading zeros, zigzag strips sign jumps, absent defaults cost zero bytes |
| zstd | frame-level **transport compression** (optional second layer) | can the byte stream get any shorter | cross-entry repetition patterns (repeated attr_id, structurally similar entries) |

The argument chain (five points):

1. **Semantics cannot be derived from bytes**. A varint stream contains no "TEAM-visible", no "client_predicted", no "inherits from Monster" — this information requires a machine-readable declaration, i.e. the contract. Conversely, protobuf produces no semantics either: what an `AttrDelta`'s attr_id points to, and whether the receiver may trust it, are defined entirely by the contract.
2. **Bytes should not be handwritten twice**. §1's current state is exactly the counterexample: the server handwrites codec + each client handwrites ByteBuffer; protocol drift is misalignment. Contract → generate → all clients aligned is the very essence learned from KBE (§2.1); protobuf merely lands "what to generate" onto an encoding target with a mature toolchain — it replaces handwritten encoding, not the contract.
3. **One source of truth, artifacts for both ends**. Contract → apollo_gen → `.proto` (golden) → protoc → C#/TS/C++ serialization layers — contract changes propagate from a single point, clients align at compile time, the same discipline as §3's CI diff gate.
4. **Hash aligned at a single point**. `schema_hash` governs semantic compatibility (handshake, §6); protoc artifacts align per build (CI-pinned versions). The two layers are each monotonic and never invent version mechanisms for each other.
5. **The ecosystem comes free**. Correct implementations of varint/zigzag/packed + mature runtimes on every client are supplied by protoc (the protobuf library is already a vcpkg manifest dependency) — a self-developed codec forfeits the operational dividend that "one mistake is searchable across the whole web".

Corrections of common misreadings:

| Misreading | Correction |
|---|---|
| "Contract and protobuf are either-or" | the contract is the source; proto is one of its artifacts (alongside C++ headers/JSON/docs, §3's five artifact classes) |
| "protobuf is compression" | varint/zigzag are **compact encoding** (removing intra-field redundancy: leading zeros, sign jumps), unaware of cross-field/cross-entry repetition; repetition patterns belong to zstd |
| "With zstd, why protobuf" | zstd cannot compress "decoding" — the client wants a strongly-typed field stream, not a byte string. protobuf provides structure, zstd reduces transport bytes; orthogonally stacked |
| "sync masks/predict must be encoded into the wire" | neither enters (§10.2.4/§10.2.6) — the wire carries only "what value was sent", not "why it was sent" |

The two-layer model:

```
┌─────────────────────────────────────────────────────────────────┐
│  Semantic layer (def contract = the single source of truth)     │
│  attrs.xml    attributes: id/name/type/sync mask/predict/default│
│  messages.xml messages: id/dir/channel/fields                   │
│  entities.xml entities: single inheritance (flattened at        │
│               generation time, architecture-review §16.7.2)     │
│  errors.xml   error-code partitions                             │
│  —— decides: who sees (mask)/who writes (predict)/when to send   │
│     (IMMEDIATE/throttled)                                       │
└──────────────┬──────────────────────────────────────────────────┘
               │ apollo_gen (four-layer funnel: XSD→parse→static_assert→startup, xml-generation §5)
               ▼
┌─────────────────────────────────────────────────────────────────┐
│  Wire layer (artifacts, golden checked in)                      │
│  apollo_contract.h/.json (delivered) + apollo_contract.proto (P2)│
│  —— decides: how values are encoded (varint/zigzag/fixed/len/   │
│     packed)                                                     │
└──────────────┬──────────────────────────────────────────────────┘
               │ protobuf serialization (protoc runtimes on each client)
               ▼
┌─────────────────────────────────────────────────────────────────┐
│  Frame layer (net-abstraction §3/§5.5)                          │
│  FrameFilter chain: zstd(>256B) → [encryption P3] → L1 16B      │
│  header+CRC32C                                                  │
│  —— decides: how the byte stream goes on transport (compression/ │
│     checksum/delimiting), independent of payload content        │
└─────────────────────────────────────────────────────────────────┘
```

### 10.1 protobuf's "Compactness" Boundary — Compact Encoding ≠ General Compression

Key encoding mechanisms (the foundation of the mapping rules):

- **Tag interleaving**: every field = `tag | value`, `tag = (field_number << 3) | wire_type` — fields interleave naturally in order of appearance, no alignment padding.
- **varint**: 7 bits per group with a high continuation bit — `uint32 v=1` encodes as 1B; leading zeros are eaten.
- **zigzag**: `sint32/sint64` first maps `(n << 1) ^ (n >> 31/63)` to non-negative, then varint — both `-1` and `1` are 1B, eliminating the 5B/10B disaster caused by sign-bit jumps.
- **Default zero bytes (proto3)**: fields equal to the default value are **not encoded** — "field-level optional" semantics for free (absent = default/unchanged, exactly the shape delta sync needs, §10.4).
- **packed repeated**: numeric repeated fields store one tag for consecutive entries — header cost amortized across entries in the same container.

**Clarification (same scope as the README)**: all of the above is **intra-field** compaction. protobuf knows nothing about cross-field, cross-entry, cross-frame repetition patterns — attr_id=1 appearing 100 times among 100 `AttrDelta`s is encoded entry by entry by protobuf without dedup; redundancy at that layer belongs to zstd (§10.5). So "protobuf+zstd" is not duplicate investment but two orthogonal layers: the former governs structural entropy, the latter repetition entropy.

### 10.2 def → proto Schema Mapping Rules

#### 10.2.1 Two Paths: the Generic Delta Pair vs Strongly-Typed (per-attribute field numbers)

**Verdict: the hot delta path uses the generic `(attr_id, value)` pair (`AttrDelta`), no per-attribute fields generated; the strongly-typed projection (accessors/SDK containers/debug tools) is the only place to assign fields per attribute, with field number ≡ attr id.**

The two paths side by side (taking an `hp` change as the example):

```
Delta path (hot path, settled in attribute-sync §7.1; this section pins "generated from the contract"):
  message AttrDelta { uint32 attr_id=1; sint64 value=2; uint32 change_n=3; }
  → encoding: tag(1B) attr_id=1(1B) tag(1B) value(1-3B zigzag)…
  → schema fixed at 3 fields, never bloating with attribute count; attr_id domain = the contract's full set (generated enum validation)

Strongly-typed path (per-client containers/server accessors/P3 debug protos):
  field number = attr id (1:1)
  → hp's field number is always 1; adding an attribute = a new id = a new field, naturally conflict-free
  → eliminates the "contract id ↔ proto field" indirection table — misalignment-class defects
    structurally nonexistent
```

Why the delta path does not adopt "one message with all attributes + one field per attribute, setting only changed fields" (the per-field optional scheme):

| Comparison | per-field optional message | AttrDelta generic pair (adopted) |
|---|---|---|
| schema size | 500 attributes = a giant message with 500 fields | fixed 3 fields |
| Receiver | generated N-branch switch / reflection-style traversal | table-driven generic loop: id → container set |
| Single-attribute change frame | tag+value ≈ 3-10B | ≈ 4-12B (1-2B more) — amortized by packing/merging when the hot path carries many per frame |
| change_n diagnostic bit | nowhere to put it | naturally the 3rd field |
| Snapshot/delta isomorphism | two shapes | the same message (snapshot sets a flag, attribute-sync §7.1) |

The encoding tax is negligible, buying a stable schema and generic decoding — **the delta path adopts the generic pair**.

#### 10.2.2 Type Mapping Table

| def type (aliases first expanded to primitives) | proto type | wire | Notes |
|---|---|---|---|
| bool | bool | varint | 1B |
| int32 | sint32 | varint(zigzag) | numeric semantics that can go negative |
| int64 | sint64 | varint(zigzag) | unified with the AttrDelta.value value bucket |
| float | float | fixed32 | coordinates/heading; protobuf does no floating-point varint, always 4B |
| double | double | fixed64 | always 8B |
| string | string | LEN | |
| bytes | bytes | LEN | |
| string[] | repeated string | LEN×N | |
| int64[] | repeated sint64 | LEN(packed) | a consecutive varint stream |
| alias (percent/timestamp…) | **zero special-casing**, mapped by the underlying primitive | — | basis points are always non-negative yet still go sint64 — zigzag spends 1 extra bit on non-negative values as an **encoding tax**, buying "aliases produce no second value-bucket shape"; special-casing would instead break AttrDelta's single sint64 value bucket |
| nested object (P2+ as needed) | nested message | LEN | a msg field type referencing another msg name → message reference; the first contract batch has no such shape |

#### 10.2.3 The Trade-off of attr-id Segmentation and Message Splitting

Current attribute-ID segmentation is documented in the `sdks/contract/attrs.xml` comments: 1-99 basic / 100-199 combat / 300-399 status / 600-699 appearance / 700-799 social / 800-899 markers. Three candidates:

| Option | Shape | Verdict |
|---|---|---|
| A one message for all attributes (per-attribute fields, §10.2.1's strongly-typed path) | generates one AttrSet | **an optional P3 artifact** (editor/debug-tool golden), not a runtime path |
| B one message per segment (AttrsBasic_1_99 / AttrsCombat_100_199…) | projection grouped by segment | **rejected** — encodes "segments" into the wire vocabulary: a contract attribute-segmentation adjustment (semantic-organization evolution) would force a schema change; segments are organization for designers/reviewers, not an encoding organization |
| C segmentation never enters the wire (delta/snapshot both use the generic pair) | segments are just contract comments + partitions of the generated enums | **main path adopted** — the wire is decoupled from the segmentation table; adjusting segments means zero wire changes |

#### 10.2.4 The sync Mask: Not in the Wire, No Message Splitting, No optional for Visibility

The mask is 7 bits (contract_model.hpp:19-27): APPR 0x01 / PROP 0x02 / SELF 0x08 / TEAM 0x10 / GUILD 0x20 / WORLD 0x40 / IMMEDIATE 0x80 (DB 0x04 forbidden in the contract, belongs to storage.xml — decision #5). **Verdict: the mask produces no proto schema element whatsoever.** Its three consumption points are all on the semantic side:

1. **Collection-time domain trimming** (attribute-sync §3.2/§4.2): the per-viewer visible set = mask ∩ viewer relationship — decides which attr_ids **enter** this frame's delta;
2. **Generated-accessor assertions** (SDK layer): compile-time visibility/write-permission checks;
3. **Documentation/validation** (§3 artifact 4).

Reasons not to split messages per tier / not to use per-field optional for visibility: (a) within one broadcast tick, SELF+TEAM+AOI attributes changing together is the norm; splitting tiers = sending multiple messages; (b) the receiver now has two application paths; (c) mask evolution (adding a tier) would change the schema — violating "the wire carries only what was sent"; (d) the domain-trimmed (id,value) set **already is** the tier's result; "absent = invisible or unchanged" is the delta stream's natural semantics.

```
Same entity, three viewers, same tick (hp=PROP, level=PROP TEAM WORLD, gold=SELF):
  viewer=self   delta = {hp, level, gold}      ← SELF∪TEAM∪WORLD∪AOI all visible
  viewer=teammate delta = {hp, level}          ← TEAM relation holds; gold trimmed away
  viewer=bystander delta = {hp, level}         ← WORLD tier visible; if hp were SELF, only level remains
  —— three AttrBatches with different content, one encoder; the client has no mask concept ——
     what arrives is authoritative
```

#### 10.2.5 Inheritance: Flattened into the Projection, Not into proto (aligned with §16.7.2)

The entities.xml single-inheritance chain `Monster ← Avatar ← Player` is expanded at generation time (parents first, children override, same-name type changes banned, with provenance — architecture-review §16.7.2); **all three consumers see only the flattened result** (attribute-sync §7.3). The proto layer therefore **has no inheritance concept**: in the attribute stream an entity appears only as `entity_id + template_id`; the client knows the attribute set from the template. Both candidates rejected:

| Candidate | Shape | Verdict |
|---|---|---|
| Flatten into one message per entity (PlayerMessage containing all inherited + own fields) | one strongly-typed message per entity | **only optionally for the P3 toolchain** (same as §10.2.3 A); not used by the runtime attribute path |
| Nested reuse (`Player { Monster base = 1; Avatar ext = 2; … }`) | proto nesting expresses inheritance | **rejected** — decode semantics would depend on nesting presence/field-shadowing rules, cross-client generation gets complex, and it directly conflicts with "flatten at generation time; the projection sees only the flat result" (§16.7.2: expansion logic enters no runtime module) |

#### 10.2.6 The predict Permission Bit: Validation-Layer Semantics, Zero Wire Bytes

`predict` (none / client_predicted / server_authoritative) decides: (1) the Lua-writable whitelist (§5 → scripting-lua §5.2); (2) client prediction-mixer behavior (attribute-sync §9: which attributes may run ahead locally, which are purely authoritative); (3) server validation strength. On the wire there is **no marker whatsoever of "this is a predictable attribute"** — prediction is a client-local behavior driven by the generated constant table (PredictableAttrs); "can it be predicted" is static semantics, never varies per frame, and costs no bytes.

### 10.3 Generator Pipeline: apollo_gen's proto Backend

```
 sdks/contract/*.xml + apollo.xsd
        │
        ▼
 ┌──────────────────────────── apollo_gen (standalone binary, decision #4: not in the runtime link graph)
 │  parse → validateAndResolve → canonical order (sorted inside contract_writer)
 ├──────────────────────────────────────────────────────────────
 │  Artifact 1  C++ header (delivered: sdks/cpp/generated/apollo_contract.h)
 │  Artifact 2  JSON (delivered)
 │  Artifact 3  protocol docs/error-code tables (§3 artifact 4)
 │  Artifact 4  schema_hash constant (same value on every client)
 │  Artifact 5  apollo_contract.proto (new in P2: fixed message family + MsgId enum + file header carrying the hash)
 └──────┬───────────────────────────────────────────────────────
        ▼  checked-in golden (same discipline as .h/.json: CI byte-diff gate, same shape as apollo_gen_golden_check)
 apollo_contract.proto
        │
        ▼  protoc (version locked by the vcpkg manifest; CI runs it at a single point)
 ┌────────────┬────────────┬────────────┐
 │ C++ (cpp)  │ C# (unity) │ TS (cocos/ │   ← per-client serialization layers; merged with layer 1 Codec of
 │            │            │    laya)   │     the four-layer responsibilities (§4)
 └────────────┴────────────┴────────────┘
```

- **proto is a golden artifact, not generated on the fly at build time**: a contract change must commit the `.proto` together (the CI diff blocks "changed the contract, forgot to generate"); developer environments need no protoc — only the release CI does.
- **Syntax proto3**: default zero bytes give field-level optional (§10.1); the file-header comment carries schema_hash + generator version + DO NOT EDIT; the reserved field-number range 19000-19999 (protobuf-internal reservation) is declared off-limits in a comment — the contract's segmentation tops out at 899, no intersection.
- **schema_hash covers the proto layer**: hash = SHA-256(canonicalBundle + generatorVersion). The proto artifact is determined by the same canonical bundle ⇒ **a semantic change necessarily changes the proto golden ⇒ the hash necessarily changes**; there is no crack of "proto changed, hash didn't"; introducing the proto backend itself bumps generatorVersion ⇒ the hash changes ⇒ the handshake graduals through the §6 dual-version window (N/N-1). protoc artifacts align not by hash but by CI pinning (the vcpkg manifest locks the protobuf version; every client's serialization layers come from the same protoc run).
- **The per-client hash constants are the same value in four places**: C++ `kSchemaHash` / C# `SchemaHash` / TS `SCHEMA_HASH` / the proto file-header comment — generated at a single point; hand-copying forbidden.

**v3 backend extension and toolchain division of labor (finalized 2026-09-29 as a correction; the final ruling is at the end of §10.6)**: the artifact list is corrected from ".pb.cc + .bin dual backend" to — `.proto` (golden, the bin's source) + `descriptor.bin` + `contract.lua` (the server's Lua contract table) + `semantic.json` (the client-semantics small piece) + `contract_route` (the server C++ frame-routing manifest: msg id → name/dir/domain/Lua handler binding — dispatch-by-id, the uplink whitelist, domain filtering; shared by both decode cases in the §10.6 sub-section) + thin client-side constants (AttrIds/error codes) + protoc/pbjs client code (emitted at a single CI point, golden checked in); business messages' `.pb.cc` is no longer produced by default (except the built-in code of the framework's fixed message family — its schema doesn't change with the contract, see §10.6's message dichotomy). Generator-internal shape clarification: **no self-built AST** — the pugixml DOM is the parse tree (the direct dividend of choosing XML over a custom DSL); the self-built core is the IR (ContractModel + four-layer validation), and each artifact is a thin "read the IR → render text" writer (no optimization passes, no semantic transformations); C# message classes are emitted by protoc `--csharp_out` (an off-the-shelf generator, not a self-developed template). Tool-distribution discipline: **zero toolchain for frontend devs** — this section's existing "golden checked in, protoc only in CI" discipline extended to all artifacts; client plugins only pull artifacts and import them (generated code into the project, bin/semantic into the resource directories, registered in the manifest), never building a second contract-parsing implementation (preventing the §1-criticized multi-client drift from rising from the dead); the act of changing the contract naturally happens in the server repo (apollo_gen is one of the CMake targets); contract.lua is a Lua table-literal dump of the IR (data, not code — no compilation step; optional luac precompilation ships with the Lua 5.4 distribution); a precompiled single binary for fast local iteration is an optional refinement, not a requirement.

### 10.4 Delta Sync Cooperating with protobuf

The full chain (per server broadcast tick, phases 4-5 of the six in attribute-sync §3.2/§10):

```
 owning scene thread (single writer)
 ┌───────────────────────────────────────────────────────────────┐
 │ history ring[(seq, attr_id, value)]  ←─ written by simulate/recalc │
 └──────┬────────────────────────────────────────────────────────┘
        ▼ collect (lock-free read of history)
 per-viewer: delta = history[(acked_seq, world_seq]] trimmed by mask
        │   ├─ IMMEDIATE(0x80) bit → skip batching, send this tick
        │   │                     (attribute-sync §5.3 INSTANT, same shape)
        │   ├─ min_interval/priority → tier throttling (the mask decides "does it
        │   │                        enter"; protobuf only encodes "what the
        │   │                        entered look like")
        │   └─ budget/watermark → attribute-sync §5 × net-abstraction §4.2
        ▼ assembly
 AttrBatch{entity_id, base_seq, repeated AttrDelta deltas, snapshot?}
 AttrSyncFrame{repeated batches, frame_ms}   ← at most one frame per client per tick
        ▼ protobuf encoding (§10.2 mapping)
 payload ──► zstd filter(>256B) ──► L1 frame header(flags) ──► transport
        ▼ client, in reverse
 deframe → decompress → decode → table-driven application: attr_id → container set (SDK layer 2,
        │   the prediction mixer behaves per §10.2.6's PredictableAttrs table)
        ▼
 AttrAck{entity_id, seq} piggybacked on any uplink ──► advances ViewerState.acked_seq
 (history can't cover the peer's progress → snapshot-reset self-healing, attribute-sync §3.3)
```

- **Delta granularity = field-level optional**: each AttrDelta "present means changed", absent means unchanged — proto3's default zero bytes make this layer's semantics free (§10.1); frame metadata (frame_ms etc.) likewise.
- **Independent per-viewer batches** (the cost of attribute-sync §4.2): the same encoder is reused; only the (id,value) set differs — protobuf is unaware of this; correctness is closed on the collection side.

### 10.5 The zstd Boundary: FrameFilter Chain Position, Threshold, Stacked Benefit

Position (net-abstraction §5.5's FrameFilter plugin slot; filters only do byte↔byte transforms):

```
 [business object AttrSyncFrame]
      │ protobuf serialization (structural entropy: varint/zigzag/packed/default zero bytes)
      ▼
 [payload bytes]
      │ zstd filter —— only when payload > 256B (repetition entropy: cross-entry patterns)
      ▼
 [compressed payload]
      │ encryption filter (P3 optional, ordered after compression: compressing ciphertext gains nothing)
      ▼
 [L1 frame: 16B header(magic|len|seq|ch/flags|crc32c) + payload]
      └─ a flags bit marks "this frame is compressed" —— the filter stack is declared by contract (frame-header ver field +
         messages.xml channel declarations → the generator emits the assembly code, net-abstraction §5.5)
```

- **The compression flag converges into the frame-header flags** (the sole authority): the filter stack is self-describing; the receiver unwraps in reverse per the frame header, consistent with "the stack order is locked by contract". attribute-sync §7.1's `AttrSyncFrame.zstd` bool is marked transitional-compat, to be converged and deleted in P2 — a duplicated two-layer flag is a drift source.
- **Basis for the >256B threshold**: zstd's minimum frame overhead ≈ 9-13B (magic 4B + frame header 1-2B + block header 3B + optional checksum 4B) + one call's CPU; movement frames (xyz+yaw four floats + header) are normally &lt;100B — compression is guaranteed negative; multi-entity attr batches and snapshots normally exceed the threshold. The threshold is per-channel configurable, benchmarked at P1 landing (together with attribute-sync §11's metrics).
- **Stacked-benefit criteria** (when compressing after protobuf still pays):

| payload shape | redundancy protobuf can't remove | expected zstd benefit |
|---|---|---|
| single entity, 1-3 deltas (10-30B) | no cross-entry patterns | **negative — don't compress** |
| multi-entity batch (>256B) | attr_id repeated across entries, batch structures similar across entities, high zero bytes in values | **high — compress** |
| full snapshot | consecutive fields + dense defaults | medium-high — compress |
| movement frames | always small | never compress |

  The verdict formula: protobuf removes **intra-field** redundancy (leading zeros/sign jumps), zstd removes **cross-entry** redundancy (repeated patterns) — the two are orthogonal; compression pays only when the latter exists. The strongest cross-frame repetition pattern is per-viewer delta-frame structural similarity — **zstd dictionary training is left for P3 evaluation**, not in the first version.

### 10.6 Compile-Time Generation vs Runtime Interpretation: the Route Trade-off Answered Head-On

This is the **real fork** between BigWorld and KBEngine, and the most common challenge to the "generation route" ("changing the contract requires recompiling"). The two families' measured shapes:

- **BigWorld**: .def → generated C++ code (entity_description parses it into the compilation artifacts) — zero hot-path overhead, strongly typed; the cost is contract changes going through recompilation (doc 36 §2 :200: evolution = change file, restart).
- **KBEngine**: .def parsed at runtime + a Python object model carrying it (entitydef.cpp:188-210, C-50) — hot reload in theory; the cost is hot-path table lookups + Variant generic types, and its twin defects on the storage face (complex types blob-ified, the contract/storage coupling's five shortcomings — attribute-sync §8, decision #5) are of the same family as runtime interpretation: **both trade compile-time determinism for runtime genericity**. Note: KBE's "hot reload" never materialized into no-downtime evolution in either family's practice (doc 36 :200: change file, restart, no version handshake) — the hot-reload dividend promised by runtime interpretation was never actually collected.

**Route C: the descriptor reflection pool (a technique practiced before, evaluated head-on).** At build time apollo_gen emits a `FileDescriptorSet` binary (`descriptor.bin` — the standard artifact of protoc `--descriptor_set_out`, **data not code**); at runtime, startup loads it into a `DescriptorPool` + `DynamicMessageFactory`, constructing and encoding messages dynamically from the descriptors. First, draw the essential line vs B: C is **not** "a mild version of runtime interpretation" — the .bin is canonical data validated and finalized by protoc at build time; "what to generate" is already pinned at build time, and at runtime there is no second chance to interpret source files; loading only deserializes and rebuilds — an illegal descriptor or a dangling field reference is a startup error, never a silent default. It still belongs to the "finalized at build time" family; what changes is that **the client-side consumption shape goes from code to data**. The boundary is equally clear: descriptors describe only the wire layer — semantic-layer content like masks/predict/segmentation does not enter the .bin; semantic consumers (the Lua whitelist, the prediction mixer) keep going through the existing JSON golden (§3 artifact 2) and generated constant tables; the two artifact layers align exactly with §10.0's two-layer model.

The three routes compared (A pure compile-time generation / C descriptor.bin reflection pool / B runtime .def parsing):

| Dimension | A pure compile-time generation (BW route) | C descriptor.bin reflection pool | B runtime .def parsing (KBE route) |
|---|---|---|---|
| Contract loading shape | apollo_gen produces C++/C#/TS/.proto offline, into the build | apollo_gen emits a **data file** (.bin) at build time; the client loads it at startup into a DescriptorPool | parse raw XML at startup to build a runtime attribute table |
| Contract-change cost | recompile the server binary (incremental, minutes) + re-ship every client SDK | **the client side swaps one .bin file and is done** — the host program doesn't recompile | swap the file, restart (in practice the same downtime window as BW, plus runtime-misalignment risk) |
| Hot-path overhead | generated code, direct field access, zero table lookup, zero dispatch (baseline 1×) | reflection encode/decode is **2-5× slower** than native (per-field Reflection-interface calls); prototype clone can amortize descriptor lookups, still slower than A | per-attribute table lookup + Variant type dispatch (a multiplier on millions of attribute accesses per tick) |
| Validation timing | the four-layer funnel moved up front: XSD→parse→static_assert→startup (xml-generation §5) | **errors at load time** — an illegal descriptor fails DescriptorPool construction; red at startup | only seen at runtime (silent defaults on misspellings are a BW/KBE shared defect, decision #3) |
| Multi-client support | one code-generation backend per client (C#/TS/C++ templates maintained separately) | **the same .bin is readable by any client with a protobuf runtime/binding stack** — JS/C#/Lua bindings all covered, zero per-client generation | every client writes its own XML interpreter; the drift surface is N-fold |
| Generator complexity | emitting code: templates/syntax/idioms maintained per client | **emitting data is an order of magnitude simpler than emitting code** — generate .proto, then `protoc --descriptor_set_out`; protoc does the generator's work | no generator (the cost is pushed onto every client's interpreter) |
| Type shape | strongly typed (struct/accessors; misuse is a compile error) | weakly typed (DynamicMessage, fields fetched by name/number; misuse surfaces only at runtime) | generic Variant |
| Failure surface | red at CI/compile; a sick build never ships (decision #4) | red at load; semantic-layer validation isn't in the descriptor, still backstopped by the generation-side gate | late startup failure / runtime data misalignment |

**Tiered usage** (first-pass scope; the v3 correction is in the final-ruling paragraph — the hot path moved to strong typing built into the framework's fixed message family, reflection stays out of the hot path):

- **Hot path** (server tick): go A — generated code, zero overhead; if some hot path has no choice but C, use prototype clone (cache the prototype at startup via `factory.GetPrototype()`, construct messages via `prototype->New()`) to amortize descriptor lookups, but still budget by the 2-5× encode/decode tax.
- **Cold path and clients** (GM backends, debuggers, internal tools, offline analysis; the client SDK in full): pure reflection suffices — the client decodes **its own** single view stream (not the server's million-entity traversal); 2-5× is within the client-side budget; in exchange: **zero generated code, zero recompiles** follow-up speed.

**schema_hash computed directly over the .bin — the multi-client consistency advantage**: under route A the hash = SHA-256(canonicalBundle + generatorVersion), and the canonical bundle is an apollo_gen-internal concept — each client can only "receive the constant and trust it", unable to independently recompute and verify; under route C the hash can be **anchored directly to descriptor.bin's bytes** (protoc's deterministic serialization guarantees the same input yields the same bytes) — "the wire description a client receives" and "the hash used in the handshake" are same-source; any client can recompute and verify on its own after loading the .bin (self-verification), and multi-client consistency upgrades from "trust the generator" to "verifiable from bytes". Landing shape: the .bin, like the .proto, is a checked-in golden (same discipline); the hash constant is produced in the same batch as the .bin (§10.3 artifacts 4/5 side by side).

**Route C's hot-update fit (each runtime's dynamic-loading API, verified)** — descriptor.bin goes through each runtime's standard dynamic-loading entry, no private format:

| Runtime | Dynamic-loading entry |
|---|---|
| C++ | `FileDescriptorSet::ParseFromString` + `SimpleDescriptorDatabase` feeding a `DescriptorPool` |
| C#/Unity | `Google.Protobuf.Reflection` (the descriptor reflection surface; `FileDescriptor` built from a byte string) |
| Lua | lua-protobuf / pbc's `pb.load` (the domestic-client hot-update standard) |
| JS/TS | protobuf.js `Root` (after loading a descriptor, full reflection is available) |
| Python | `descriptor_pool.AddSerializedFile` (ops scripts/offline tools consume the bin directly) |

Engineering shape: **descriptor.bin enters the hot-update pipeline as an ordinary resource file** — Unity AssetBundle / mini-game subpackages / CDN version directories; swapping the bin neither recompiles the engine nor goes through store re-shipment (the resource-update channel); field-number compatibility (rebuttal ① below) guarantees new/old bins and new/old servers coexist within the N/N-1 window. Two engineering caveats:

- **(1) C++ `DescriptorPool` descriptors are not unloadable** — the correct shape of hot reload is **generational replacement**: a new bin version builds a new pool; once existing messages' lifetimes end, the old pool is discarded wholesale; no in-place descriptor add/remove (undefined behavior).
- **(2) Reflection is 2-5× slower than native** — the tiering is unchanged: hot path compile-time generated (A); cold path/clients/tools on the bin (C).

**The existing decision chain stands on "finalized at build time", not on "A exclusively"** (not newly established in this section): decision #4 — the generator is a standalone binary, outside the runtime link graph, and failure blocks release; architecture-review §0 rejected the runtime-container shape — runtime interpretation of the contract is exactly "runtime containers" mirrored onto the data plane; the first contract batch already landed static_assert compile-time gates (modules/contract/tests/gen_compile_test.cpp, layer 3); decision #11's comparison note (UE's DOREPLIFETIME is a C++ macro; changing the protocol means recompiling — apollo moved the mask declaration from code into contract data; changing a mask = changing XML + regenerating, no handwritten code touched; "runtime-configurable" means the declaration's location is datafied, not runtime interpretation). A and C are both in the "finalized at build time" family — the .bin is emitted by apollo_gen at build time, validated by protoc, and read-only at runtime (after loading, no source files are interpreted, no structure changed), the same discipline as architecture-review §0's "finalized at startup, read-only at runtime", outside the "runtime container" criticism's blast radius; the A/B/C three-route verdict is in the final ruling at this section's end.

Three rebuttals to "changing the contract requires recompiling":

**(1) proto field numbers' forward/backward compatibility — evolution ≠ all clients recompiling in sync.** field number ≡ attr id (§10.2.1); adding an attribute = a new field number, and **old clients skip unknown fields per protobuf rules (unknown-field preservation), decode unbroken**; new clients reading old clients' missing fields take defaults. Combined with the hash handshake's N/N-1 dual-version window (§6), "adding a field"-class evolution only requires the server to upgrade first and clients to follow on their release cadence — no coupling of "change one attribute and every client stops for a recompile". The only things truly needing all clients in one cycle are field deletion/semantic changes (§6 change discipline) — which should sweep all clients anyway.

**(2) The real cost of recompilation is overestimated.** What recompiles is **the server binary and the client SDK artifacts, not the engine** — under apollo's modular layout the contract artifacts link only `modules/protocol` and `sdks/*`; incremental compilation is minutes. And an MMO's **structural-layer** contract changes (adding attribute segments, adjusting segmentation) naturally come with storage.xml migrations, gradual rollout, announcements, and maintenance windows — inherent parts of the change process; "recompiling" rides along, it is not an extra cost. The genuinely high-frequency changes (numbers, copy, event parameters) go through ConfigRegistry and Lua (decision #12) and never touch the contract.

**(3) Layered design leaves a door for high-frequency changes — structural layer generated, business layer dynamic.**

```
 Structural layer (low-frequency, stable): attrs.xml segments 1-899 —— generates C++/C#/TS, zero overhead, strongly typed
 Business layer (high-frequency changes): two no-recompile channels
   (a) generic container field: the contract reserves a map<uint32, bytes>-shaped attr (e.g. ext_data) ——
       contents are key-values interpreted by Lua/config; on the wire it is just a bytes; proto knows nothing
   (b) configId references: business-content tables drive it (skills/items/quests all in DB tables, doc 36 §2
       Mangos precedent :142) —— the contract transmits only ids; content evolution = zero wire change
 Conclusion: business-change-without-recompile is carried by (a)+(b)+the Lua whitelist (§5);
       "changing protocol structure requires recompiling" turns from a defect into a quality gate
       (compile time keeps mistakes out of production).
```

**Final ruling (2026-09-29, corrected same day to v3): the codec route = reflection by default, code as the enhancement on both ends.**

The first-pass ruling was "A+C dual track, C primary" (A narrowed to the server hot path, C the client default). The correction was triggered by two new inputs: (1) the **client-side code hot-update channel** (Unity HybridCLR's C# DLL hot updates, Cocos/Laya's native TS script hot updates) removes A's number-one cost (store releases) — compile-time rigor and hot updates become compatible, so an all-C client bundle is no longer necessary; (2) the **server zero-recompile requirement** retires the business messages' .pb.cc projection. The v3 scope:

- **Key patch — messages split into two classes**: the **framework's fixed message family** (movement/attributes/control channels: AttrDelta/AttrBatch/move/heartbeat…) has a schema that **does not change with the contract** (what changes is the attr_id domain/enum validation, not these messages' shapes) → strong-typed code is generated once and **built into the framework**; contract changes never trigger its recompilation; **business messages** (events and other channels) have schemas that change with the contract → bin reflection. The reflection tax lands only on business messages (a single server: 5000 CCU, a few messages per person per second ≈ 10k messages/s; DynamicMessage decodes at µs scale; CPU share negligible); **zero reflection on the hot path**. The boundary goes into the contract: messages.xml gains `binding="native|reflect"` (default by channel: movement/attributes/control=native, events=reflect).
- **Server: zero recompiles on contract change**. It loads descriptor.bin (full) + contract.lua (the Lua contract table: attr table/message routes/whitelist — the same semantics content in dual form, require-and-use); business messages go DynamicMessage ──sol2 bridge (iterating descriptor fields and transferring, fixed framework code written once, never changing with the contract)──► Lua tables ──► Lua handlers; the attribute container is table-driven (id → AttributeValue, built from contract.lua). A contract change = swap the bin + swap contract.lua + hot-update the Lua handlers, **C++ untouched**. DescriptorPool generational hot updates (caveat (1) above: a new bin gets a new pool; the old pool is discarded once existing messages' lifetimes end).
- **Clients: tiered by code hot-update capability**. Clients with hot-update pipelines take **generated code** (protoc `--csharp_out` → HybridCLR hot-update assemblies; TS via pbjs/ts-proto → script hot updates) — compile-time type checking plus hot updates; **bin as the fallback channel** (UE5 P3/tools/GM/debug/rapid new-client onboarding/teams not doing code hot updates) — zero generated code, zero compilation; at load time semantic.json verifies reference integrity (every attr id/field name referenced by business exists), catching the big risks.
- **Load-time consistency gate (the rigor source moves from compile time to load time)**: at server startup/hot-update the three artifacts cross-check — bin bytes recomputed ↔ internal_hash anchoring (protoc pin); the bin's message set ↔ the contract.lua route table **aligned item by item** (bin has it, Lua has no handler = startup red — KBE misses only at runtime, §11.1); whitelist attr ids ↔ contract.lua existence. The manifest mechanism is one and the same (§11.6). The essential difference vs KBE runtime negotiation: **build-time-finalized artifacts + a load-time gate** vs runtime self-attestation without a source of truth.
- **Generator shape**: apollo_gen emits in one batch `.proto` (golden, the bin's source) / `descriptor.bin` / `contract.lua` / `semantic.json` / thin client-side constants / protoc/pbjs client code (§10.3 v3 paragraph); business-message `.pb.cc` is not produced by default (except the framework family's built-in code). The schema_hash mechanism is unchanged (§6's algorithm anchors to canonicalBundle + .bin bytes for self-verification; the two states agree).
- **Rationale of the ruling**: the two slowest links in the evolution chain (server C++ recompiles, client store releases) are **simultaneously** decoupled from the protocol; rigor doesn't depend on compile time — it is carried by "verify everything at load"; hot-path rigor and performance are secured by the framework family's dichotomy.
- **Batch impact**: the bin/lua backend + the sol2 reflection bridge + the validation gates + the client code wrapping are **implemented in the same batch** (§8 P2 already re-scoped); route B remains rejected. Compatible with the decision chain: the bin is finalized at build time and read-only at runtime (outside architecture-review §0's "runtime container" criticism); the two-layer model (§10.0)/the four domain-separation rules (§11.3)/the manifest (§11.6) all retained; decision #12 deepened — Lua is promoted from business script to the runtime carrier of contract semantics (the whitelist is still generated from the contract; the carrier is named as contract.lua).
- **Honest costs (recorded into the batch review)**: the sol2 reflection bridge's maintenance surface (packed repeated/nested recursion); no compile-time types in the Lua handler layer (contract golden replay tests backstop regressions, consistent with decision #12's existing shape — no degradation); the binding dichotomy's boundary needs contract review to hold the line; a new fixed C++ module (descriptor loading + reflection codec + the validation gate).

#### Where Business Messages Decode: C++ Bridge vs Lua-Side Decoding (two cases; an assembly choice, not a contract fork; added 2026-09-29)

> **2026-09-30 update: sol2 is dropped (upstream maintenance stalled)** — every "sol2 bridge" below reads as "**C-API transfer bridge**": the bridge was always self-written fixed generic transfer code (next paragraph clarifies that this is not a sol2 capability); dropping sol2 only swaps the "transfer into Lua tables" function family from sol2 templates to the native Lua C API (the `lua_newtable`/`lua_pushinteger`/`lua_settable` family), with magnitude and maintenance surface actually reduced. The "sol2 bridge" wording in the historical passages is kept as written (the v3 ruling's record); this note governs execution; scripting-lua §2 was revised in the same batch (**the Lua 5.5 line rides vcpkg + native C API binding, ruled** — version policy re-revised 2026-09-30: lock the 5.5 main line, patch position follows the vcpkg lua port, currently 5.5.x).

Once v3 made business messages bin-reflected, one assembly-level choice remains: **whether reflection decoding happens in C++ or Lua**. First, a naming misunderstanding: the "sol2 bridge" is not sol2's dynamic-binding capability — sol2's usertype binding is compile-time templates (each field hardcoded), useless for DynamicMessage whose structure is known only at runtime; the "bridge" means **a self-written stretch of fixed generic transfer code**: iterate the descriptor's fields (`GetDescriptor()->field(i)`), fetch values via Reflection (`GetInt64/GetString/…`), and transfer via sol2 into Lua tables, recursing into nested messages, expanding packed repeated into Lua arrays; the reverse direction (Lua table → DynamicMessage, for replies/broadcasts) is isomorphic — a few dozen lines, generic for every message, never changing with the contract. The "dynamic binding" effect comes from the descriptor-data-driven loop, not from any library's ready-made mechanism.

| | Case 1: C++ reflection + sol2 bridge (**default**) | Case 2: byte passthrough + Lua-side lua-protobuf |
|---|---|---|
| Decode location | C++ DynamicMessage → bridge → Lua table | C++ does frames/L1/id-based routing only; the payload bytes go to Lua: `pb.load(bin)` + `pb.decode(name, bytes)` |
| C++ message surface | **complete** — content-based auditing/fine-grained throttling/in-process GM/gateway content routing all share the DynamicMessage facility | opaque — middleware can key only on msg id/name/dir (frame-router-layer mapping, covering the vast majority of use cases) |
| Self-written code | a generic bridge, a few dozen lines (fixed) | no bridge (pb.decode returns a Lua table directly) |
| Dependencies | the protobuf main library (**already a dependency, zero additions**) | lua-protobuf (a C-implemented Lua module, active; pbc unmaintained, not chosen) — a new build dependency |
| Decode performance | C++ reflection, µs scale | lua-protobuf is a C implementation, same order of magnitude |
| Consistency-gate executor | C++ (bin ↔ route-table alignment, §10.6 final-ruling paragraph) | movable into Lua (after pb.load, enumerate types ↔ contract.lua route-table alignment) — the gate's **semantics are identical in both cases**: load-time item-by-item alignment; a miss is a startup red |

**Performance quantification (estimated scope, not a benchmark)**: case 2 is structurally one intermediate representation fewer — case 1 = wire→DynamicMessage→Lua table, **two conversions** (one Reflection getter + stack op per field); case 2 = wire→Lua table, **one conversion** (the C implementation parses and builds the table in one pass) — the difference is a ~1.5-2× constant factor, not library quality. A single 10-field message (~100B): case 1 ≈1.5-3µs, case 2 ≈0.6-1.6µs; load baseline 25k messages/s (5000 CCU × 5/s) → case 1 ~50-75ms CPU/s (a 20Hz main thread at ~4ms/frame, 8%), case 2 ~15-40ms/s (~2ms/frame, 4%) — **both within 10% of the frame budget; performance is not the deciding factor**; decoding can also be offloaded to the IO thread pool (the logic thread receives only Lua tables — equally open in both cases; chosen, the difference leaves the main thread). Memory: case 1, one DynamicMessage heap object per message (allocation churn, poolable via prototype); case 2, only Lua string+table (one fewer C++ heap-allocation layer).

**The assembly granularity can be per-process (a spectrum, not a binary)**: the gateway process takes case 2's shape (pure passthrough, zero reflection facilities) + baseapp processes take case 1's shape (decode content for audit/GM) — the contract/golden/route manifests are completely unchanged; each process differs only in whether the reflection data plane is installed — the "two cases" are really the two ends of one assembly spectrum.

The remaining dimensions, condensed: **access-layer content filtering** — case 1's gateway can decode content for pre-validation (coordinates/rates/parameter ranges intercepted in C++); case 2 pushes it down to Lua (equally trusted server-side code; security is not lost — what's lost is the gateway's independent pre-filter); **debugging** — case 1 exposes content to C++-side `DebugString()`/perf; case 2's investigation goes through Lua (one extra hop); **error handling** — case 1's decode errors are caught and dropped at the C++ frame layer; case 2's surface in Lua (`pb.decode` returns nil+err), needing handler pcall discipline; **testing** — case 1's bridge needs round-trip unit tests; case 2's decode is the library's business — contract replay covers it.

**Contract-design impact: zero contract fork between the cases.** The binding attribute keeps two values (native|reflect), governing only "framework family vs reflection" — **"where reflection decodes" is a deployment-assembly choice, deliberately kept out of the contract**; both cases share the same contract source, the same golden batch, the same descriptor.bin (what lua-protobuf consumes is also a standard FileDescriptorSet — the same format as the client bin). The only artifact-face increment is a lightweight route manifest **shared by both cases**: the C++ frame-routing layer needs msg id → name/dir/domain/Lua handler binding under either case (dispatch-by-id, the uplink whitelist, domain filtering all query it) — defined as apollo_gen's fixed new artifact `contract_route` (JSON/constant form, emitted straight from the IR; the §10.3 list already includes it).

**Switch condition (written down, against wavering)**: default case 1 — keeps C++ message-content visibility (content-based audit/GM/content-based middleware) + zero new dependencies; switch to case 2 only once it is confirmed that C++ will never need business-message content and the team prefers a thinner C++ — the switch changes the assembly (the C++ side installs no reflection data plane; the route manifest unchanged); contract and golden change zero.

### 10.7 Wrap-Up

The def-contract/protobuf relationship in one sentence: **the contract is the source; proto is one of its most important projections; the semantic layer (masks/predict/inheritance/segmentation) never enters the wire; the wire layer (varint/zigzag/default zero bytes) never generates semantics; zstd at the frame layer removes the cross-entry redundancy protobuf cannot**. Multi-client consistency closes the loop via "single golden + schema_hash + protoc pin + the load-time consistency gate"; the route is ruled **reflection by default, code as the enhancement on both ends** (§10.6 v3: the framework's fixed message family carries the hot path with built-in strong typing — its schema doesn't change with the contract and never recompiles for the contract; business messages go bin-reflection through the sol2 bridge into Lua, and the server recompiles zero on contract change; clients with code hot-update pipelines take generated code, with bin as fallback — the two slowest links, server recompiles and client releases, decoupled simultaneously, with rigor carried by the load-time consistency gate); business dynamism is carried by the three doors of the generic container field + configId + Lua — the protocol's structural stability is no longer a sacrifice of evolution speed, but immunity to "shipping sick".

---

## 11. Internal/External Contract Domain Separation: Registering and Designing Around the KBE "No Internal/External Contract Separation" Defect (appended 2026-09-29)

> Defect source: found by the user's hands-on KBE code reading (line numbers verified by direct reading of this session's shallow-clone working copies). The avoidance design's finalized scope: **domain = a logical property on messages, not a physical file** — the contract's physical organization is not mandated; the only hard rule is that the domain-segmentation logic (ID segmentation + dual hash + per-domain filtering + the cross-domain prohibition) must hold, orthogonal to physical organization. Domain separation applies to messages only (attrs' SELF/TEAM/GUILD/WORLD are "within-client" visibility-domain subdivisions, not this section's internal/external split; server-private data goes through internal messages or the storage layer).

### 11.1 Defect Registration: KBE's Missing Internal/External Contract Separation (grade-A evidence)

**One file mixes messages for three audiences** — `kbe/src/server/baseapp/baseapp_interface.h`:

| Audience | Messages (line numbers) | Marker |
|---|---|---|
| To clients | `hello` :99-100, `loginBaseapp` :144-145, `logoutBaseapp` :150, `reloginBaseapp` :156-157, `onClientActiveTick` :112-113, `reqAccountBindEmail` :264, `reqAccountNewPassword` :289 | `BASEAPP_MESSAGE_EXPOSED` |
| To dbmgr | `onDbmgrInitCompleted` :90 | none (internal) |
| To cellapp | `onMigrationCellappStart` :326, `onMigrationCellappEnd` :331 | none (internal) |

**EXPOSED is just an export marker, not namespace isolation** — `baseapp_interface_macros.h:27-36`: `BASEAPP_MESSAGE_EXPOSED(NAME)` expands to `NETWORK_MESSAGE_EXPOSED(Baseapp, NAME)`, affecting only the "may the client channel call this" check, participating in no ID-allocation semantics.

**The root disease: message IDs share a single allocation table** — every handler (regardless of audience) registers into the same table via `MessageHandlers::add` (`kbe/src/lib/network/message_handler.cpp:139`); `msgID_` is a single auto-incrementing counter (`message_handler.h:116` `lastMsgID() {return msgID_ - 1;}`, `:133`); `FixedMessages` (`fixed_messages.cpp`) can pin fixed ids for individual messages, but only as scattered points, not audience-segmented. Consequence: **any internal message's add/remove shifts the subsequent ID layout** — internal evolution and client SDK regeneration are welded together; internal and external cannot evolve independently. Whether files are mixed is only the symptom; **the undivided ID space is the root disease**.

**Runtime negotiation masks the missing contract split**: `importClientMessages` (`baseapp.cpp:4859`→`:4891`, `loginapp.cpp:1416`→`:1469`) — after the client connects it requests, and the server dynamically ships the entire message table (names/ids/parameter types) down via `ClientInterface::onImportClientMessages` (`client_lib/client_interface.h:174`); the client builds the table at runtime. The message-table content is the contract, yet no build-time source of truth exists — one negotiation per connection; "both ends agree" rests on runtime self-attestation, not build-time locking.

### 11.2 The BigWorld Comparison: Same Disease, Different Shape (Mercury Message-Table Verification)

- **Declaration-layer mixing, same shape**: .def mixes `<ClientMethods>`/`<BaseMethods>`/`<CellMethods>` sections in one per-entity file (Account.def, recorded in architecture-review §16.2) — client-visible methods and server-internal methods, same file, same source.
- **The ID space is likewise a single table**: Mercury has one `InterfaceMinder` table per process; `add()` allocates IDs sequentially by `elements_.size()` (`lib/network/interface_minder.cpp:38`); the table cap is 255 (`interface_minder.hpp:35` `interfaceElement(uint8 id)`). Client-callable exposed methods go through `ExposedMethodMessageRange` (`lib/entitydef/method_description.hpp:31`) calling `addRange` (`interface_minder.cpp:53-70`, dividing remaining space into 1/x shares) to occupy a contiguous reserved segment **inside the same table**.
- **Reading**: BW is one step ahead of KBE — it converges exposed methods into a reserved segment whose internal add/remove doesn't shift other messages' layout in the table; but the reserved segment itself is partitioned by registration order within a single table, and the root disease — internal messages and exposed methods sharing one ID space — is of the same type (and the uint8 cap makes the space tighter). **Both families, same disease: a single ID-allocation table undivided by audience.**

### 11.3 The Avoidance Design: Domain-Segmentation Logic (hard rules, orthogonal to physical organization)

All four rules land on the generator/XSD/hash mechanisms, independent of how files are split:

**(1) The ID space is segmented by domain.** The messages' msg-id space (independent of attr id) is split into two segments: the client domain and the internal domain each take one (example: client 1-899 / internal 900+, segment values set at landing, XSD locks the segment boundaries — following the attrs.xml id-segmentation discipline, the contract attribute-ID segmentation rules). **Within a segment, free add/remove, neither shifting the other's layout** — the direct counter to KBE's single-allocation-table root disease. Landing shape: msg elements carry a `domain="client|internal"` attribute (xs:enumeration); the id/domain segment constraints go into the XSD.

**(2) schema_hash is computed per domain, twice.** `client_hash` covers only the client domain's canonical bundle; `internal_hash` covers only the internal domain (algorithm as §6, input filtered per domain). An internal change does not alter client_hash — **the client handshake stays stable, the bin isn't re-shipped**; the handshake uses client_hash (§6's N/N-1 window semantics unchanged, its object narrowed to the client domain); internal_hash serves only as a same-batch assertion at server deployment (same batch rebuilt, same batch started; no gradual-rollout window needed). Landing scope (P2 implemented): the client bundle = client-domain messages + attrs + errors (one-to-one with the client artifact face's content); the internal bundle = internal-domain messages + entities; **the hand-written version does not enter a domain bundle** — the version travels with any change; if it entered a bundle, an internal-only change would also shift client_hash and the handshake stability would fail (the full identity, version included, is still anchored by schema_hash). The companion carry-surface discipline is in §11.6.

**(3) apollo_gen filters projections per domain.** The same contract source yields two artifact faces: the client bin (and TS/C#) **projects the client domain only**; the server `.pb.cc` is full — one source file, two artifact faces, **the single source of truth unbroken, diffs concentrated** (changes are always reviewed in one place). Domain filtering and §10.6's ruled bin projection are the same mechanism, implemented in the same batch.

**(4) The XSD cross-domain reference prohibition.** A client-domain message's parameters referencing internal-only types (including internal-domain messages/internal structs) errors immediately — the domain boundary goes into the validator, not left to reviewer vigilance; the same layer as §2.3's xs:key/xs:keyref discipline.

Evolution cadence is thereby decoupled (the direct corollary of (2)(3)): an internal-domain change = the server rebuilds and deploys in the same batch; a client-domain change = field-number compatibility + bin hot-update (§10.6's ruled resource pipeline) — the two change classes never implicate each other.

```
            ┌─────────────────────────────────────────────┐
            │ sdks/contract (single source of truth; physical organization free, §11.4) │
            │ messages: msg carries a domain attribute + ids segmented by domain        │
            │   client segment 1-899       internal segment 900+       │
            └────────────┬──────────────────┬────────────────┘
                         │ apollo_gen per-domain filtering (one mechanism, two projections)
          ┌──────────────▼───────────┐    ┌──▼──────────────────────┐
          │ client_hash              │    │ internal_hash           │
          │ = SHA-256(client-domain  │    │ = SHA-256(internal-domain│
          │   canonical bundle)      │    │   canonical bundle)     │
          └────────────┬───────────┘    └──┬──────────────────────┘
                         ▼                   ▼
          client bin / TS / C#            server .pb.cc (full)
          client domain only              internal change → same-batch rebuild
          internal change → hash unchanged    client change → field-number compat
          → handshake stable, bin not re-shipped      + bin hot update (§10.6)
```

### 11.4 Contract Physical Organization: a Factual Record of Three Shapes (nothing mandated)

Physical organization is left to the consumer. The generator's format layer provides **include aggregation** — pugixml has no built-in XInclude; the expansion is self-implemented in apollo_gen's reading layer: read the root file, recursively expand references into a single document tree (zero new dependencies), for those who want split files. XSD validation and schema_hash are always computed over **the aggregated whole**; physically splitting files does not affect hash stability. The facts on three organization shapes:

Landing note (P2 batch five implemented): `expandIncludes` expands the four file classes uniformly at the directory-level reading layer (parseContractDirectory) — includes are recognized only at the root's direct child level (the XSD pins the position at the start of the root's children; deep includes are intercepted by the strictWalk whitelist); an included file's root element must have the same name and version; cycles/missing files/root mismatch/version mismatch/missing href each have a dedicated diagnosis; duplicate includes are allowed to concatenate (duplicate keys are caught by the existing rules); "a split-file directory and a single-file directory are completely equal in model and schema_hash" is locked by tests.

| Organization shape | Facts | Domain-segmentation logic (hard rules) |
|---|---|---|
| single file + domain attribute | contract v1 is exactly this shape (just add the domain attribute); diffs most concentrated; no expansion layer | holds — segmentation reads id+domain, not the file |
| multiple files + include aggregation | client.xml / internal.xml (or per-module) maintained separately, a root file references and aggregates; once the message count grows, diffs can split per review domain | holds — after expansion it's a single tree; hash/validation over the aggregate |
| two independent contracts | client/internal each with their own XML+XSD, no aggregation root | each holds on its own, but **the source is split in two** — conflicting with the "single source of truth" premise (executive summary 1, §1); the cross-domain reference prohibition and the unified diff gate are both built on a single source; adopting it means first overturning that premise |

**The only hard rule restated: the domain-segmentation logic (ID segmentation + dual hash + per-domain filtering + the cross-domain prohibition) must hold — it constrains the ID space and the artifact faces, not the file layout.**

### 11.5 Landing Batch

With the P2 proto/bin dual backend in the same batch (§8 row already added): the domain attribute and id segmentation into the XSD is a small contract-side change; per-domain filtering is the same pass of code as the bin projection; the dual hash is the direct generalization of §10.6's schema_hash two-state consistency (client_hash anchored to the client-domain bin bytes, internal_hash to the server .pb.cc batch); the client contract package (§11.6's three-piece set) is the bin backend's same-pass output.

### 11.6 The Client Contract Package and Manifest: Closing the Consistency Loop for Multi-File Delivery (appended 2026-09-29)

Filling a gap in §10.6: descriptor.bin covers only the wire layer — §10.6 already declared masks/predict absent from the .bin, with semantic consumers going through the JSON golden and generated constant tables. The client actually also needs **small semantic pieces**: the predict table (→ the prediction mixer), attr id→name (→ debugging/UI binding), the error-code table. "Multiple files" is a fact, not dodged — the solution is not per-file validation but **promoting the delivery unit from file to package**:

```
dist/client/<client_hash>/          ← the directory name is the version fingerprint (CDN version directories address by hash; cache-friendly)
├── manifest.json                   # package fingerprint: client_hash, generator version, per-file SHA-256 manifest
├── descriptor.bin                  # wire layer (protoc standard artifact; any protobuf runtime consumes it directly)
├── semantic.json                   # semantic pieces: predict table, attr id→name, error codes
└── (optional) generated .cs/.ts    # clients with code hot-update pipelines (§10.6 v3): protoc/pbjs artifacts
```

apollo_gen emits the three pieces in one batch (the same run as the .pb.cc; clients with code hot-update pipelines add a fourth: the protoc/pbjs-generated .cs/.ts — **also in the manifest's file list**, so "new code, old bin" mix-and-match is equally caught by the load-time per-file hash) — the manifest turns "same-batchness" from a verbal discipline into **client-verifiable physical evidence**. The server side is isomorphic: three artifacts cross-checking (bin + contract.lua + whitelist — §10.6 v3's load-time consistency gate). **Carry-surface discipline (P2 implemented)**: package member files carry only client_hash and the generator identity — not the full schema_hash, not the hand-written contract_version (both would be shifted by internal-only changes: the directory name client_hash unchanged while file bytes changed would break the CDN version-directory immutability); the landed semantic.json and the client-domain .proto header carry exactly this; the manifest fields follow the same scope (§11.3 (2)'s bundle partitioning pairs with it — the version does not enter a domain bundle).

The validation chain (two layers, all at load/handshake time, never in the runtime):

```
pull the package (hot-update pipeline) → read manifest → compute each file's SHA-256 and compare → any mismatch [load-time red]
               → manifest.client_hash compared against the server handshake → mismatch [handshake red (§5: reject + prompt to upgrade)]
               → all pass → bin into DescriptorPool + semantic.json into SDK layer 2
```

Every inconsistency path sealed:

| Scenario | Caught where |
|---|---|
| new bin + old semantic.json (partial hot-update failure/CDN cache mismatch) | the manifest's per-file hash — load-time red, not a runtime data misalignment |
| manifest disagrees with files (tampering/half package) | same as above, load-time red |
| a whole old-version package (bin+semantic+manifest self-consistent) | **legal** — exactly an N/N-1 window member (§6 semantics unchanged) |
| whole package new, server old | handshake red (§5) |

The key property: **version validation has exactly one point — client_hash** (§11.3 (2)'s handshake object). There is no "one version number per file" multi-point drift surface; inter-file consistency is the manifest's mechanical check — the loader is written once and every client shares the same logic.

Three "synthetic single file" candidates rejected:

| Candidate | Rejection reason |
|---|---|
| a custom container `[len][json][bin]` | every client writes a shell-stripper; forfeits route C's core advantage — the bin is a protoc standard artifact that any runtime `ParseFromString`s directly |
| semantics stuffed into .proto (custom options/enums) | semantics hard-coded into the wire description distorts §10.0's two-layer model; protoc's validation surface can't govern it |
| single-blob packaging | no incremental updates — semantic.json rarely changes, the bin changes with messages; split files re-ship only what changed; the manifest is pure JSON, isomorphic to AssetBundle/mini-game subpackage manifest conventions, zero learning cost |

In one sentence: **multi-file is real, but "the possibility of inconsistency" is closed entirely by the manifest's per-file hash + the single-point client_hash at load/handshake time, never entering the runtime.**

---

## 12. The Contract Toolchain and Workflow: Generation Flow, CI/CD, and Editor Integration (appended 2026-09-29)

> Positioning: §10.3 v3 fixed the generator shape and the distribution discipline; this section writes the **end-to-end flow explicitly** — every step from changing the contract to it taking effect on both ends, each step's tool and failure catch-point, and the phased provisioning of the CI/CD jobs and editor integration. Discipline red line: **all tools only consume apollo_gen and its artifacts; a second contract-parsing implementation is never written** (immediate XSD validation excepted — that is the standard XML Schema mechanism, executed by the editor extension, not self-developed).

### 12.1 The End-to-End Generation Flow (who, with what, where mistakes are caught)

```
(1) Edit the contract    server developers (the people changing contracts are naturally in the server repo)
   sdks/contract/*.xml —— VSCode + an XML extension associated with apollo.xsd
   → live validation/completion/enum hints (red squiggles while editing, zero self-development, §12.3's zero-cost layer)
(2) Generate locally     cmake --build --target apollo_gen && apollo_gen sdks/contract
   → re-emit all golden: .h/.json/.proto/descriptor.bin/contract.lua/
     semantic.json/thin client constants and client code wrapping
   → four-layer funnel (XSD→parse→static_assert→startup pre-checks, xml-generation §5)
(3) Commit               contract sources + all golden in one commit (a miss is caught by (4))
(4) PR check             CI re-runs apollo_gen → byte-diff against committed artifacts
   → mismatch = red ("changed the contract, forgot to generate" blocked before merge — the executor of §3's existing discipline)
(5) Release packaging    CI (on main merge): protoc (vcpkg pin) finalizes → assemble
   dist/client/<client_hash>/ (manifest+bin+semantic+optional code, §11.6)
   + internal-side deployment pieces (bin+contract.lua) → upload to the artifact registry/CDN version directory
(6) Server load          the deployment script swaps the bin + contract.lua
   → the load-time consistency gate (§10.6 v3) → N/N-1 window gradual rollout (§6)
(7) Client update        the hot-update pipeline pulls dist/client/<client_hash>/ (manifest verification)
   or code hot update (HybridCLR assemblies/TS scripts)
```

| Step | Owner | Tool | Failure catch-point |
|---|---|---|---|
| (1) Edit | server developer | VSCode + XSD association | while editing |
| (2) Generate | same | apollo_gen (a CMake target) | the four-layer funnel |
| (3) Commit | same | git | — (caught by (4)) |
| (4) PR check | CI | apollo_gen re-run + byte-diff | before merge |
| (5) Package | CI | apollo_gen + protoc (pin) | before release |
| (6) Load | ops/deployment script | the consistency gate | at server startup |
| (7) Pull | client plugin | manifest verification | at client load time |

Frontend devs have zero toolchain (§10.3 v3's distribution discipline): (7) only imports; (1)-(5) always happen in the server repo — the distribution problem of "compiling/generating software" structurally does not exist in this flow.

### 12.2 CI/CD Tools (provided in P2)

| Job | Trigger | Action | Blocks what |
|---|---|---|---|
| `apollo_gen_golden_check` | every PR | re-run the generator, byte-diff all golden | changed the contract, forgot to generate/hand-edited artifacts |
| `contract_pack` | main merge | protoc finalizes → assemble the `dist/client/<client_hash>/` package (manifest+per-file SHA-256) → upload to the artifact registry/CDN | release-artifact consistency — CI is the manifest's **issuer** (the machine evidence of same-batchness) |
| `schema_hash_report` | when a PR contains contract changes | a PR comment: hash N→N+1, the changed domain (client/internal), the affected message/attribute list | review visibility — whether this PR touched the client contract is visible at a glance |
| `compat_window_check` (P3) | before release | spin up minimal instances for each of the two hashes N/N-1 and run the contract replay tests | a new contract breaking the old gradual-rollout window (§6 semantics) |

Landing note (P2 delivered the `contract_pack`/`schema_hash_report` jobs, 2026-09-29): `scripts/ci/contract_pack.sh` + workflow `.github/workflows/contract.yml` — assembles the packages from **already-committed golden** (no rebuild: gate two has already guaranteed golden matches the source), both packages shipped together: `dist/client/<client_hash>/` (manifest+descriptor.bin+semantic.json) and `dist/server/<schema_hash>/` (manifest+descriptor_full.bin+contract.lua+contract_route.json — the deployment unit of §10.6 v3's load-time three-artifact cross-check); the manifest carries domain hashes + the generator + the **protoc version** + per-file SHA-256 (no contract_version — carry-surface discipline §11.6/§11.3 (2)); protoc pinned 29.3 (changing versions = re-pack and re-sign, the version in the manifest for traceability). `scripts/ci/schema_hash_report.sh` — reads two refs' committed golden and produces a three-hash comparison/domain-change verdict/message-attribute set diff; PR comments are idempotent (the bot edits the same comment); the two scripts share one logic locally and in CI, reproducible on this machine. Package-level immutability is measured: after an internal-only change + version bump, the client package's three files (manifest included) are byte-for-byte unchanged while the server package changes.

### 12.3 Editor Integration: Three Progressive Layers (tiered by cost)

| Layer | Provides | Self-development cost | Batch |
|---|---|---|---|
| **Zero-cost layer** | the VSCode XML extension (Red Hat) + the contract files' header `xsi:noNamespaceSchemaLocation="apollo.xsd"` (+ the in-repo `.vscode` association config) → live XSD validation, tag/attribute completion, enumeration hints | **zero** — the standard XML Schema mechanism | P1, usable with contract v1 |
| **Scaffolding layer** | in-repo `tasks.json`: "generate contract" runs apollo_gen in one click; "contract change preview" shows the artifact/hash/domain changes corresponding to this diff (rendered from apollo_gen's JSON output) | config files, no extension development | P2 tail |
| **Plugin layer** | a VSCode extension: contract tree view (attrs/messages/entities browsed by domain), id↔generated-code/doc jumps, change-impact analysis (which messages reference an attribute), compact-style snippets (§2.3), right-click regenerate | extension development (TS) — **only calls the apollo_gen binary/reads its JSON artifacts, never parses contracts** | P3, on demand (when the scaffolding layer falls short) |

The plugin-layer discipline, restated: the extension is a **view** over apollo_gen, not a second parser — all semantics come from the generator's artifacts; "the single source of truth" is unbroken on the tool side too.

### 12.4 Landing Batch Summary

- **P2**: `apollo_gen_golden_check` (upgraded to byte-diff over all artifacts) + `contract_pack` + `schema_hash_report` + the VSCode scaffolding layer (tasks.json). (The first three delivered: the golden gate runs as the ctest `apollo_gen_golden_check` (ci.yml matrix); contract_pack/schema_hash_report run via `contract.yml` + `scripts/ci/`; the scaffolding layer goes with later batches.)
- **P3**: `compat_window_check` + the VSCode plugin layer (on demand) + the apollo_gen precompiled single-binary distribution (the fast-local-iteration refinement, §10.3).

---
