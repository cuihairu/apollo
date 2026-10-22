# SDK Contracts and Message Encoding

XML/XSD contracts are the source for generated metadata. Validate contracts, wire encoding, transport, and business outcomes separately. Generated files do not establish runtime integration.

## Executive summary

Parsing, generation, hashes, domain filtering, inheritance expansion, slice declarations, and golden checks exist. Some applications still use MessageCodec/M1 translation. Handshake compatibility windows are integrated; version-specific business decoding, complete client upgrades, and production rollout acceptance remain in todo.md.

## Protocol and code-only updates

Protocol changes affect contracts or actual wire representation and require regeneration, compatibility classification, and release/rollback acceptance. Code-only updates preserve contracts and wire: Lua uses prepare/commit/canary/rollback; C++ uses process replacement after drain. Gateway alone does not establish seamless updates.

## 1. Current integration boundary

`sdks/` contains contracts, generation, C++ outputs, and SDK code. `skds/` still contains platform SDK work. Directory and consumer convergence remain incomplete and require validation against real consumers.

## 2. Contract model

### 2.1 Inputs

`sdks/contract/` contains attrs.xml, entities.xml, messages.xml, errors.xml, version, and apollo.xsd. `modules/contract` parses and validates; `sdks/gen` builds the generator.

### 2.2 Types

WireType supports bool, int32, int64, float, double, string, bytes, string lists, and int64 lists. Aliases name existing primitives rather than adding wire types. Illustrative nested-object designs do not establish implemented support.

### 2.3 Attributes and inheritance

Attributes declare ID, type, visibility mask, channel, prediction permissions, and optional slice metadata. Slice capacity/type are paired and validated. Single inheritance expands at generation: ancestors first, child override, no same-name type changes. Runtime consumers receive flat attribute sets.

Storage flags do not belong in wire contracts. Server persistence owns fields, tables, and journals; client visibility masks do not control saving directly.

## 3. Generated artifacts

| File | Purpose |
|---|---|
| apollo_contract.h | C++ identifiers and metadata |
| apollo_contract.json | JSON projection |
| contract.lua | Lua contract and binding metadata |
| semantic.json | Client semantic projection |
| contract_route.json | Routing and field layouts |
| apollo_contract.proto | Full protobuf schema |
| apollo_contract_client.proto | Client-only protobuf schema |

The generator stays outside business runtime dependencies. Applications consume outputs; clients must not implement independent contract parsers.

## 4. SDK responsibilities

Separate codecs, attribute baseline/delta state, entity/AOI lifecycle, and presentation events. Generated APIs can follow C++/C#/TS conventions while preserving identical message and field semantics.

## 5. Server integration

Validate direction/domain against routing metadata and bind registered handlers. Lua permissions and message entry points use controlled metadata. Network callbacks do not mutate Scene directly. Validate existing World frame translation and script consumers explicitly.

## 6. Versions and compatibility

Generated artifacts contain schema, client, and internal fingerprints. Client packages expose only client definitions. Hashes detect differences but do not alone classify compatibility.

InterServerLink accepts the current route_version and compat_route_versions. APOLLO_ROUTE_COMPAT configures the window using prior schema hashes; peers declare acceptance in both directions and reject versions outside the window. peer_route_version() exposes the selected peer generation. This does not provide version-specific application decoders: consumers must validate field layout and business semantics for each encoding.

Adding fields is not automatically compatible: current positional encoding, protobuf, and business defaults can differ. Classify changes against actual encoding and semantics, not only XML diffs.

## 7. Storage boundary

Wire types, visibility, and message versions do not define tables. Profiles, journals, and future demand-driven database integrations evolve through their own migrations. storage.xml is a declarative-statement design direction, not a current business storage consumer.

## 8. Current work

The root todo.md is the only implementation backlog. Complete SDK convergence, encoding migration, and rollout acceptance separately without another historical phased roadmap.

## 9. Related designs

- [XML generation](./xml-generation.md)
- [Attribute synchronization](./attribute-sync.md)
- [Lua](./scripting-lua.md)
- [Network boundaries](./net-abstraction.md)

## 10. Semantics, encoding, and compression

### 10.1 Semantics and bytes

Contracts declare meaning, direction, domain, and permissions. Encoding produces bytes; compression processes those bytes. Encoding does not establish authorization.

### 10.2 Field mapping

Types, lists, order, and defaults follow the generator and matching decoder. Validate handwritten MessageCodec conversions rather than treating similarly named structures as identical wire formats.

### 10.3 Toolchain

apollo_gen emits text/metadata; protoc produces descriptors during packaging. Schema generation, loadable descriptors, and actual runtime consumption are separate outcomes.

### 10.4 Attribute deltas

Baselines, sequences, and projection follow attribute synchronization. An encoding library does not manage observer progress or missing baselines.

### 10.5 zstd

Optional frame compression needs thresholds, decompression limits, versions, and performance validation. Current gameplay-path integration is not claimed.

### 10.6 Native and reflect

Messages have native/reflect binding metadata with channel-based defaults. Lua can consume generated routing and semantics, but a reflect flag does not establish a complete protobuf-reflection runtime. Current Lua bindings use the C API.

### 10.7 Release boundary

Changing a Lua handler without changing contracts is code-only. Fields, messages, layouts, and semantics require protocol review. Code-only updates still require state compatibility, delivery, and rollback acceptance.

## 11. Domains and delivery packages

### 11.1 Domains

Client IDs occupy 1–899; internal IDs occupy 900–65535. Validate direction and domain separately. ID ranges alone cannot authorize client requests for internal server operations.

### 11.2 Stable identity

Do not repurpose published IDs, field meanings, or encoding. Application renaming must not silently alter the active ABI. Intentional ABI changes follow protocol versioning.

### 11.3 Domain fingerprints

client_hash describes client definitions, internal_hash describes internal definitions, and schema_hash describes the full contract. Client packages exclude internal definitions; servers load matching packages/fingerprints.

### 11.4 File organization

Physical files serve maintenance without changing logical domains. One parser generates shared models; clients do not create separate interpretation rules.

### 11.5 Loading

Validate package identity, versions, and per-file digests before binding routing/handlers. Incomplete or mixed-version outputs fail explicitly. Replacing one file does not complete a protocol release.

### 11.6 Current packaging

contract_pack.sh uses protoc to package client descriptor.bin + semantic.json and server descriptor_full.bin + contract.lua + contract_route.json. Manifests record generator/protoc versions, domain fingerprints, and file digests. Packaging success does not establish compatible rollout.

## 12. Development and CI

```bash
apollo_gen --contract sdks/contract --out sdks/cpp/generated
apollo_gen --contract sdks/contract --check sdks/cpp/generated
```

XSD validation, semantic tests, and golden checks constrain input/output consistency. Compatibility, old/new interoperability, and rolling release/rollback need separate acceptance. VitePress locales keep each language in its own pages.
