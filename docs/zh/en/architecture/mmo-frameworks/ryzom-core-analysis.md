
# Ryzom Core Architecture Analysis

## 1. Positioning

Ryzom Core represents another MMO architecture tradition that is very valuable:

- Multi-service decomposition
- Long-lived commercial MMO style
- Service meshing
- MMORPG-specific algorithms and network-layer optimization

It is more "service-oriented" than TrinityCore and a more "industrial service cloud" than KBEngine.

## 2. Core Architecture

The official service architecture docs list a large number of services:

- `AES`
- `AS`
- `AIS`
- `BMS`
- `EGS`
- `GPMS`
- `IOS`
- `NS`
- `WS`
- `TS`
- `MS`
- `SU`
- `FES`

This kind of naming looks old-school, but in essence it shows it adopts:

- Explicit dedicated service decomposition
- Naming and discovery services
- A Tick sync service
- An AI service
- Front-end access services
- A global position service

## 3. MMO Core Capability Analysis

### 3.1 Service Decomposition

Ryzom Core's core is not a single worldserver, but "one shard carried out by a group of cooperating services".

This is a very typical commercial MMO operations-grade line of thinking.

### 3.2 Network and the Variable Database

The official features docs mention:

- Bit-packed UDP
- Fixed-rate connections
- packet loss management
- tree structured network variable database
- XML-defined network variable database

This means it is not simply sending protobuf packets; a stronger MMO-specific sync modeling exists.

### 3.3 Geographic Regions and AI

`AIS` handles geographic-region AI and `GPMS` handles global position, which shows it has explicit service-oriented modeling for space and entity position management.

## 4. Distinctive Traits

- Extremely strong service-mesh decomposition
- Very much like a traditional commercial MMORPG operations architecture
- The network layer is strongly targeted at MMO traffic characteristics
- Has a "variable database" abstraction that leans more MMO than ordinary room servers

## 5. Strengths

- Very valuable as a reference for decomposing a complex MMORPG's systems
- Very fine-grained service responsibilities
- Fits studying MMO production-grade service layout

## 6. Weaknesses

- Noticeably dated architecture
- High cost to understand and get started
- Material is scattered; the ecosystem is less active than mainstream modern frameworks

## 7. Applicable Scenarios

Good fit:

- Studying large MMORPG service decomposition
- Analyzing the dedicated-service layout of traditional commercial MMOs
- Referencing an old-school mature scheme that is "not a single world server"

## 8. Lessons for Apollo

What is most worth absorbing:

- Service boundaries such as naming / tick / front-end / AI / position
- Splitting MMO core responsibilities into long-running, explicit services
- The variable database and sync modeling ideas

## 9. References

- Service architecture: https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884873/Ryzom%2BService%2BArchitecture
- Project structure: https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884861/Ryzom%2BCore%2BProject%2BStructure
- Feature overview: https://ryzomcore.atlassian.net/wiki/spaces/RC/pages/884824/Ryzom%2BCore%2BFeatures
