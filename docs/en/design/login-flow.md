
# Login Flow

This document describes login boundaries in the room-based model. See the [Architecture Overview](../guide/architecture.md) for target topology.

## Responsibilities

- `Login` authenticates accounts and issues short-lived login credentials.
- The client uses the credential to establish a game connection to `Gateway`.
- `Gateway` validates the credential and establishes a connection session, then routes messages to the RoomServer currently hosting the player.
- After accepting a join-room request, the RoomServer creates or restores the player's match runtime object; it is the sole authority for match state.

Account authentication, client connections, and match simulation are separate responsibilities. A login credential proves that account authentication succeeded; it does not decide which process owns the player object. The server must validate room admission and routing information. Clients cannot select the authoritative RoomServer themselves.

## Typical flow

```text
Client → Login: account authentication
Login → Client: login credential
Client → Gateway: establish game connection and submit credential
Gateway → RoomServer: validate session and route room-join request
RoomServer → Gateway/Client: accept join and return initial state
```

The implementation design owns credential format, handshake encryption, expiry, replay protection, queueing, and account storage. Do not describe unimplemented details as current capabilities.

## Reconnection and transfer

When the same account logs in again, the old session must be invalidated or explicitly replaced. Reconnection restores session identity, but may establish a new connection. Cross-server transfer is defined in [Sessions and Online Routing](./session-and-online-directory.md): the RoomServer completes authority handoff first, then Gateway routing is updated.

## Implementation status

The repository contains `login-app` and `gateway-app` prototypes. End-to-end login, Gateway credential validation, RoomServer routing, and cross-server takeover still need validation against this flow. Base/Cell routes in prototype code do not define the target flow.
