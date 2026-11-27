# Apollo Lua Property/Table Change Awareness: Option Comparison

> 2026-11-27 user directive: research the candidate options, lay out the trade-offs, and do
> not decide on the user's behalf.
> Source: external review `docs/review/2026-11-27-lua-table-tracking.md` (ChatGPT design),
> compared against this repo's `docs/zh/design/{scripting-lua,attribute-sync,lua-attr-sync}.md`
> and the source of `modules/game/attrsync`, `modules/script`,
> `modules/game/world/src/scene.cpp`.
> The external review is a proposal, not a fact about this repo; the "repo status" entries
> below were checked against the source.
> Scope: property/table change awareness and sync for the **Lua runtime** only. Editor-side
> static typing (pyi) belongs to kode and is out of scope here (it was mistakenly included
> once and has been removed).

## 0. Problem definition (what exactly has to be solved)

After a Lua script mutates a property or table field, the engine must:
1. **Perceive**: know which field of which entity changed (including deep nesting
   `self.bag[3].level = 4`)
2. **Translate**: turn it into an Apollo attribute change (attr_id + value + visibility
   domain) feeding the existing attrsync
3. **Deliver**: DeltaBuilder → FrameCodec → client (this chain already works)
4. **No collateral damage**: plain Lua tables pay no extra cost; handles stay valid across
   GC; rawset/C API writes are not missed

The difficulty lives entirely in steps 1 and 2 — 3 already exists, and 4 is where the
options differ in cost.

## 1. Option set (four)

### Option A: status quo — C++ host writes + Lua hook-window revision

Mechanism: C++ writes the attribute → ChangeHistory/dirty → the Scene opens a hook window
at frame end → `ScriptRuntime::run_attr_hook_window` invokes the Lua
`onAttrChanged(entity, attr, old, new)` → Lua returns a revised value which is written
back → `apply_attr_hook` records it through the normal path. **Lua cannot initiate
writes; it can only revise values the host wrote.**

- Repo status: **implemented and tested** (`test_attr_sync_attr_hook.cpp` /
  `test_scene_attr_hook_bridge.cpp`)
- Pros: zero engine change; write whitelist + frame budget + `lua_pcall` protection +
  errors keep the original value — the safety boundary is complete; shares the attrsync
  chain, no second source of truth
- Cons: **Lua cannot initiate attribute changes** (gameplay scripts that want to change
  hp have to round-trip through C++); payloads are only int64 quadruples, **no deep
  structure**; hooks are batched at frame end, not real-time

### Option B: declarative write API (lua_write_whitelist + path arguments)

Mechanism: inject `apollo.attr.set/add/get`, internally calling the regular
`AttrSyncManager` write path; depth is expressed explicitly with path arguments such as
`apollo.attr.skills(entity, 3, "level", 4)`. Free-form table references are not exposed.

- Repo status: **design** (`docs/zh/design/lua-attr-sync.md`), not implemented
- Pros: small change (three injected C functions); write-notifies, reusing the existing
  dirty/ChangeSink/hooks; the whitelist can guard core authoritative attributes; path
  arguments make depth incrementally dirtyable (only leaf ids)
- Cons: **gameplay script writing style is constrained** (cannot naturally write
  `self.hp = 8500` or `self.bag[3].level = 4` — the API must be called); the path grammar
  needs per-container design work; depth limits must be enforced

### Option C: BigWorld-style PropertyOwner bubbling

> **Minimal demo runs**: `demos/lua-tracking/demo-c-owner.cpp`
> (`./demos/lua-tracking/run.sh c`) — verifies owner-chain bubbling plus path-based deep
> increments; outputs `entity.slot[3]`.

Mechanism: a script write → `DataType::attach(value, owner, index)` → container values
are themselves `PropertyOwnerBase` subclasses → child changes bubble
`onOwnedPropertyChanged(change)` up the parent chain to the Entity → dispatched by the
attribute's flags. Deep changes are a path sequence (leaf→root index chain) plus the
leaf value, delivered bit-packed.

- Repo status: BigWorld has a complete implementation
  (`lib/entitydef/property_owner.*` / `property_change.*`); **apollo does not**
- Pros: an industry-proven deep-change model; path+leaf-value increments are precise (no
  whole-table resend); flags (OWN/OTHER/GHOSTED/BASE/PERSISTENT) decide dispatch at
  declaration time
- Cons: **requires porting the whole PropertyOwner/DataType attach architecture**
  (apollo is currently int64+slice with no owner chain); Python setattr interception
  does not translate directly to Lua; large effort

### Option D: sidecar metadata + mutation observer (external review's recommendation)

> **Minimal demo runs**: `demos/lua-tracking/demo-d-coverage.lua`
> (`./demos/lua-tracking/run.sh d`) — exercised four write paths on real Lua 5.5.1 and
> pinned down "without touching the interpreter source, capture cannot be complete":
> `__newindex` misses assignments to existing keys / rawset / C API; `debug.sethook`
> snapshotting can cover them but is slow.

Mechanism: no change to Lua table layout; weak-key tables (Lua-side liveness) + a C++
IdentityMap (fast identity lookup) + a Metadata Pool (sidecar data) + Generations (guard
against handle reuse); **modify the Lua 5.5.1 C source** (ltable.c fast/slow write paths,
lvm.c, lapi.c, lgc.c) to observe writes → Mutation Journal → Attribute Transaction →
reuse `AttrSyncManager`.

- Repo status: external review only, **not implemented, feasibility unverified**; apollo
  uses the vanilla Lua C API and has not modified the engine's Lua sources
- Pros: captures **all writes** (plain assignment / rawset / C API / stdlib / fast
  paths); deep nesting and GC lifecycle handled; general infrastructure (reusable by
  debugger/audit/hot migration); zero cost for plain tables
- Cons: **requires forking/modifying the Lua 5.5.1 engine source** (maintenance burden,
  upgrade cost, separate builds, ABI risk); the review itself says an `lua-table-observer`
  prototype must first prove every write path is capturable; highest effort and risk

## 2. Comparison matrix

| Dimension | A status-quo hooks | B declarative API | C BigWorld owner | D sidecar observer |
|---|---|---|---|---|
| Lua-initiated writes | no | yes | yes | yes |
| Deep nested changes | no | yes (path args) | yes (path) | yes (automatic) |
| rawset/C API capture | — | no (bypasses API) | no (relies on attach) | yes |
| Write-then-notify (not frame-end batch) | no | yes | yes | yes |
| Engine changes | none | none | medium (attach chain) | **large (Lua C source)** |
| Effort | done | small | medium–large | **large** |
| Live-gameplay risk | low | low | medium | **high** |
| General reuse | low | low | medium | high (infrastructure) |

## 3. How the options relate (not a decision — possible combinations)

- A is the current foundation: the hook-verification + whitelist + budget safety boundary
  should be kept by any new option
- B is the lowest-cost way to add "active writes" on top of A; depth comes from explicit
  path arguments
- C is B's architecturally complete form: depth becomes an engine-native owner chain
  instead of an API convention
- D is the only option that does not miss rawset/C API writes, at the cost of modifying
  the Lua engine source — the review recommends proving a prototype first

## 4. Points that need the user's decision

1. Must deep nesting be supported? Scalar assignment only inside `__init__` → B is
   enough; gameplay frequently mutates `self.bag[i].field` → C or D
2. Must rawset / C API writes (paths bypassing a declarative API) be captured? Yes →
   only D
3. Is modifying the Lua 5.5.1 source acceptable? This is the biggest fork in the road
   (D's precondition)
4. Should the review's `lua-table-observer` prototype be built first (prove on real Lua
   5.5.1 that every write path is capturable)?

> This document does not choose for you. Name a direction and a landing plan for it
> will follow.
