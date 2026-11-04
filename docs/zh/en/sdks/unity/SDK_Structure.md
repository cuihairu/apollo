
# Apollo SDK Directory Structure

## Overview

In this repository the Unity SDK code lives in two trees: the client/connection/auth modules under `skds/unity/ApolloSDK/`, and the attribute system under `sdks/unity/ApolloSDK/Attributes/`; the SDK workspaces for other platforms stay under `skds/` (`cocos/`, `laya/`).

The `docs/sdks/` directory holds SDK documentation and stays consistent with the server-side framework in protocols and interfaces.

## Directory Structure

```
skds/
├── unity/                    # Unity client SDK
│   └── ApolloSDK/
│       ├── ApolloClient.cs         # main client
│       ├── ApolloClientConfig.cs   # configuration
│       ├── Network/               # networking module
│       │   └── NetworkManager.cs
│       └── Session/               # session management
│           └── AuthManager.cs
├── cocos/                   # Cocos Creator SDK
└── laya/                    # LayaBox SDK

sdks/
└── unity/                    # Unity SDK (attribute system today)
    └── ApolloSDK/
        └── Attributes/            # attribute system
            ├── AttributeValue.cs
            ├── AttributeContainer.cs
            ├── AttributeSyncManager.cs
            └── ...
```

## Unity SDK Modules

| Module | Location | Function | Status |
|------|------|------|------|
| `ApolloClient.cs` | `skds/unity/ApolloSDK/` | Main entry point: connect, authenticate, heartbeat | Done |
| `ApolloClientConfig.cs` | `skds/unity/ApolloSDK/` | Configuration class | Done |
| `Network/NetworkManager.cs` | `skds/unity/ApolloSDK/` | TCP/Socket connection management | Done |
| `Session/AuthManager.cs` | `skds/unity/ApolloSDK/` | Login authentication | Done |
| `Attributes/` | `sdks/unity/ApolloSDK/` | Attribute sync system | In progress |
| `Messaging/` | — | Protobuf message routing | Planned |
| `Utilities/` | — | Logging, timers, etc. | Planned |

## Documentation Conventions

- SDK design docs go under `docs/sdks/{platform}/`
- One `.md` file per module
- Naming convention: `{ModuleName}_SDK.md`
