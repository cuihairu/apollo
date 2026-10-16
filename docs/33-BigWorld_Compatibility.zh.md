[English](33-BigWorld_Compatibility.md) | [中文](33-BigWorld_Compatibility.zh.md)

# BigWorld 兼容层（Apollo）

状态：**增量式**。这是一个构建在 Apollo 之上、BigWorld 风格的 C++ API 门面，不是完整的 BigWorld 引擎/运行时。

## 目标

为游戏/服务器逻辑提供一套熟悉的 `BigWorld::*` C++ API 面，同时复用 Apollo 基础设施。兼容层的设计原则：

- **KISS**：一个小而明确的运行时，带更新循环
- **YAGNI**：只实现我们实际用到的 API
- **可测试**：每个对外 API 都有可运行的测试用例

## 已实现 API（现状）

头文件：

- `include/bigworld/BigWorld.h`：公开的 `BigWorld::*` 门面
- `include/apollo/bw/*`：实现类型（`Runtime`、`Entity`、id）

实现：

- `src/apollo/bw/runtime.cpp`

### 全局门面（`BigWorld::*`）

- 时间：
  - `double BigWorld::time()`
  - `uint64_t BigWorld::timeMs()`
  - `void BigWorld::update()`
- 实体注册表：
  - `registerEntityFactory(typeName, factory)`
  - `createEntity(typeName)`
  - `entity(id)`
  - `entities()`
  - `destroyEntity(id)`
- 定时器（全局）：
  - `addTimer(initialOffsetSeconds, repeatOffsetSeconds, fn, userArg)`
  - `delTimer(timerId)`
- 一次性回调：
  - `callback(delaySeconds, fn)`
  - `cancelCallback(callbackId)`

### 实体基类（`BigWorld::Entity`）

- 标识：
  - `EntityId id() const`
  - `const std::string& typeName() const`
- 实体定时器：
  - `TimerId addTimer(initialOffsetSeconds, repeatOffsetSeconds, userArg)`
  - `bool delTimer(timerId)`
- 销毁：
  - `bool destroy()`
- 生命周期钩子：
  - `onEnterWorld()`
  - `onLeaveWorld()`
  - `onDestroy()`
  - `onTimer(timerId, userArg)`

## 语义（重要）

### 更新循环

定时器只在调用 `BigWorld::update()` 时被派发。你必须在自己的主循环里周期性调用它（例如每 tick/帧一次）。

### 定时器单位与行为

- 时间参数单位为**秒**（`double`）。
- 负偏移会被钳制到 `0`。
- `repeatOffsetSeconds == 0` 表示**一次性**定时器。

定时器取消规则（有意分开）：

- `Entity::delTimer()` 取消该实体创建的**实体定时器**。
- `BigWorld::delTimer()` 取消由 `BigWorld::addTimer()` 创建的**全局定时器**。
- `BigWorld::cancelCallback()` 取消由 `BigWorld::callback()` 创建的**回调定时器**。

跨类型取消按设计返回 `false`。

### 线程与安全

当前实现为**单线程**（无锁）。请在同一个线程调用 `BigWorld::*` API。

### 异常

用户回调或实体 `onTimer()` 抛出的异常会被捕获并忽略，以保证运行时稳定。

## 快速上手

```cpp
#include "bigworld/BigWorld.h"

struct Avatar final : BigWorld::Entity {
    Avatar() : Entity("Avatar") {}
    void onEnterWorld() override { /* ... */ }
    void onTimer(TimerId id, int32_t arg) override { /* ... */ }
};

int main() {
    BigWorld::registerEntityFactory("Avatar", [] {
        return std::make_shared<Avatar>();
    });

    auto e = BigWorld::createEntity("Avatar");
    e->addTimer(0.1, 1.0, 7);

    for (;;) {
        BigWorld::update();
        // sleep/tick...
    }
}
```

## 测试

所有对外 API 由以下文件覆盖：

- `tests/test_bigworld_api.cpp`

CTest 目标：

- `BigWorldApiTests`

### API 覆盖映射

- `time()/timeMs()/update()` → `test_time_monotonic`、`test_update_required_for_callback_dispatch`
- 实体生命周期 API → `test_factory_and_entity_lifecycle`、`test_factory_invalid_inputs_and_factory_returns_null`
- 实体定时器 API → `test_entity_timer_api`、`test_addTimer_invalid_clamp_and_cross_cancel`
- 回调 API → `test_bigworld_callback_api`、`test_callback_empty_and_exceptions_swallowed`
- 全局定时器 API → `test_bigworld_global_timer_api`、`test_addTimer_invalid_clamp_and_cross_cancel`

## 尚未实现（roadmap）

该兼容层目前**不**提供：

- BigWorld Base/Cell 分离、mailbox 与分布式实体路由
- EntityDefs/IDL 与自动属性复制
- Python 脚本兼容
- 与官方 BigWorld 客户端的网络协议兼容

当你明确所需的 BigWorld API 面/版本（BaseApp/CellApp、mailbox 语义、脚本语言）后，下一步是按同样的「API + 测试」规则增量扩展本层。
