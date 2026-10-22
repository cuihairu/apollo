
# Apollo SDK Directory Structure

## Overview

In this repository the Unity SDK code lives under `sdks/unity/`; the SDK workspaces for other platforms stay under `skds/`.

The `docs/sdks/` directory holds SDK documentation and stays consistent with the server-side framework in protocols and interfaces.

## Directory Structure

```
sdks/
└── unity/                    # Unity SDK
    └── ApolloSDK/
        ├── ApolloClient.cs         # main client
        ├── ApolloClientConfig.cs   # configuration
        ├── Network/               # networking module
        │   └── NetworkManager.cs
        ├── Session/               # session management
        │   └── AuthManager.cs
        ├── Attributes/            # attribute system
        │   ├── AttributeValue.cs
        │   ├── AttributeContainer.cs
        │   ├── AttributeSyncManager.cs
        │   └── ...
        ├── Messaging/             # message handling
        └── Utilities/             # utilities

skds/
├── cocos/                   # Cocos Creator SDK
└── laya/                    # LayaBox SDK
```

## Unity SDK Modules

| Module | Function | Status |
|------|------|------|
| `ApolloClient.cs` | Main entry point: connect, authenticate, heartbeat | Done |
| `ApolloClientConfig.cs` | Configuration class | Done |
| `Network/NetworkManager.cs` | TCP/Socket connection management | Done |
| `Session/AuthManager.cs` | Login authentication | Done |
| `Attributes/` | Attribute sync system | In progress |
| `Messaging/` | Protobuf message routing | Planned |
| `Utilities/` | Logging, timers, etc. | Planned |

## Documentation Conventions

- SDK design docs go under `docs/sdks/{platform}/`
- One `.md` file per module
- Naming convention: `{ModuleName}_SDK.md`
