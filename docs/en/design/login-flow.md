# Login Flow

loginServer authenticates, gateway binds the connection, and Base activates persistent objects through database. World places gameplay; cell creates simulation entities. See the [mapping](./bigworld-apollo-mapping.md).

## Typical flow

```text
Client → loginServer: authenticate and obtain credentials/ingress
Client → gateway: connect, validate credentials, associate session
Directory: resolve owning Base, unchanged by cross-server play
baseServer → database: load profiles and confirm activation
Client → gateway → Base: persistent business or gameplay request
Base → worldServer: members, Base association, snapshot, admission
World → cell: place Space/Cell, create temporary Instance if needed
Directory/gateway: project committed World/Space/Cell/executor location
cell ↔ gateway ↔ Client: initial scene and simulation input/state
```

Servers validate home scope, versions, capacity, credentials, and members. Clients cannot assign profile ownership or storage. Base objects can exist before gameplay entry. World does not keep another simulation entity; cell does not load complete profiles.

## Reconnection and implementation

Duplicate login revokes old sessions. Reconnection restores original Base and current simulation location without another profile writer. Authentication, ingress, and directory primitives exist; Base/database prototypes provide remote storage; default login routing, World/cell separation, and real recovery remain unfinished. Test profiles/injected sessions are not production authentication evidence. Track work in root todo.md.
