# XML Contract Generation and Loading

Current XML consumption primarily belongs to build-time contract parsing/generation, not a generic runtime configuration container. Sources live in modules/contract, sdks/gen, and sdks/contract.

## Executive summary

pugixml parses the model; the contract module validates semantics and computes hashes; apollo_gen emits artifacts. CI checks XSD and golden outputs. Runtime services consume generated metadata without linking the generator. Typed gameplay-table loaders, declarative storage statements, and their replacement paths require separate integration.

## 1. Current consumers

| Component | Fact |
|---|---|
| sdks/contract | Attributes, entities, messages, errors, version, and XSD |
| modules/contract | Parsing, validation, inheritance expansion, hashing, and tests |
| sdks/gen | apollo_gen and byte-for-byte output checks |
| sdks/cpp/generated | C++, JSON, Lua, routing, semantic, and proto artifacts |
| Runtime | Generated-table consumption; not arbitrary XML hot reload |

External engines' use of XML does not define Apollo's topology or storage interface.

## 2. Boundaries

XML/XSD represents maintainable, validated declarations. Wire messages remain binary, Lua handles scripting, and large geometry assets/logs do not become XML.

storage.xml, gameplay tables, and frame configuration are separate designs requiring consumers, schemas, loading validation, and failure handling. They are not all current implementations.

## 3. Generation source

Validated XML enters ContractModel. Generated code and runtime metadata are projections; SDKs must not maintain independent field/permission definitions.

Check generated artifacts and fingerprints when changing descriptions, types, fields, or semantics. A textual edit is not automatically release-neutral.

## 4. Current toolchain

```bash
apollo_gen --contract sdks/contract --out sdks/cpp/generated
apollo_gen --contract sdks/contract --check sdks/cpp/generated
```

Artifacts include apollo_contract.h, apollo_contract.json, contract.lua, semantic.json, contract_route.json, and full/client `.proto` files. contract_pack.sh uses protoc to generate descriptors and manifests with file digests. The generator does not create a database service or activate a new business protocol directly.

## 5. Validation

1. XSD checks structure, allowed types, and declarations.
2. The contract module checks IDs/domains, references, inheritance cycles, and cross-field relationships.
3. Generated headers compile; golden checks align committed artifacts with source.
4. Consumers validate package identity, digests, routes, and bindings at startup.

The first three have tools/tests; consumer-specific loading requires its own acceptance. Generation, parsing, and compatible business operation are distinct outcomes.

## 6. Reserved storage declarations

Declarative statements, startup validation, and mappings can inform future integration. Persistence stays outside client contracts. Current business saving uses local profiles, SaveQueue, and PersistJournal. Activate real SQL/Redis backends, parameter binding, retries, and migrations when deployment requirements demand them.

Mapper examples, database fields, and dependencies do not establish integrated storage.

## 7. Updates and failure

Code-only updates retain the wire. Contract/encoding changes require protocol versions, compatibility windows, and release rollback. Replacing XML alone does not establish seamless client updates.

New loaders validate complete versions before switching at the owning execution boundary. Failures retain usable versions and report explicit outcomes. Validate the actual consumer mechanism.

## 8. Related designs

- [SDK contracts](./sdk-contract.md)
- [Lua updates](./scripting-lua.md)
- [Attribute synchronization](./attribute-sync.md)
- [Application assembly](../architecture/starter-and-module-assembly-design.md)
