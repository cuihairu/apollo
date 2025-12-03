# Project TODO List

## 1. Architecture Optimization (High Priority)

- [ ] **Adopt C++20 Coroutines**

  - [ ] Refactor `Network Module` to use `co_await` instead of callbacks.
  - [ ] Refactor `Database Module` async queries to use coroutines.
  - [ ] Update `Script Module` to support Lua-C++ coroutine interoperability.

- [ ] **Enhance Data Consistency**

  - [ ] Implement **CAS (Compare-And-Swap)** mechanism for `MySQL` persistence layer to prevent race conditions.
  - [ ] Design and implement **TCC (Try-Confirm-Cancel)** framework for distributed transactions (e.g., cross-server trading).

- [ ] **Service Scalability & Reliability**

  - [ ] **WorldServer Sharding**: Split WorldServer responsibilities (e.g., separate `SocialService`, `MatchService`) or implement sharding by logic ID.
  - [ ] **GateServer Backpressure**: Implement flow control to reject new requests/connections when backend `GameServer` load exceeds thresholds.

- [ ] **Observability**

  - [ ] Integrate **OpenTelemetry** SDK.
  - [ ] Implement distributed tracing context propagation across `Gate` -> `Game` -> `DB` services.

- [ ] **Lifecycle Management**
  - [ ] Implement `IComponent` interface and `ApplicationContext` manager.
  - [ ] Refactor existing services to inherit from `IComponent`.
  - [ ] Implement `AutoRegistry` for component registration.
  - [ ] Implement **Component Discovery** (Service Locator) for type-based lookups.

## 2. Documentation

- [ ] Update `04-框架架构设计方案.md` to reflect C++20 and Coroutine usage.
- [ ] Create detailed design doc for TCC transaction flow.
