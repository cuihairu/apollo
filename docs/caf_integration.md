# CAF (C++ Actor Framework) 集成指南

> **注意**: Apollo 自研的 `actor` 模块已被移除，推荐使用成熟的 [CAF (C++ Actor Framework)](https://github.com/actor-framework/actor-framework)。

## 为什么选择 CAF？

- **成熟稳定**: 经过多年生产环境验证
- **完善文档**: 详尽的官方文档和教程
- **活跃社区**: 开源社区持续维护
- **功能丰富**: 分布式、远程 actor、监控等
- **类型安全**: 强类型消息系统

## 安装 CAF

### 使用 vcpkg (推荐)

```bash
cd /path/to/vcpkg
./vcpkg install caf:x64-windows
```

### 使用 conan

```bash
conan install caf/0.19@
```

### 从源码编译

```bash
git clone https://github.com/actor-framework/actor-framework.git
cd actor-framework
./configure
cmake --build . --target install
```

## CMake 集成

在项目的 `CMakeLists.txt` 中添加：

```cmake
# 查找 CAF
find_package(caf CONFIG REQUIRED)

# 链接 CAF 库
target_link_libraries(your_target
    PRIVATE
    caf_core
    caf_io
    caf_actor
)

# 或仅使用核心功能
target_link_libraries(your_target PRIVATE caf_core)
```

## 基本 CAF Actor 示例

```cpp
#include <caf/actor.hpp>
#include <caf/event_based_actor.hpp>
#include <caf/io/all.hpp>
#include <iostream>

using namespace caf;

// 定义一个简单的 Actor
class HelloWorld : public event_based_actor {
public:
    HelloWorld(actor_config& cfg) : event_based_actor(cfg) {
        // 启动时的行为
        printf("HelloWorld actor started\n");
    }

    behavior make_behavior() override {
        return {
            // 处理字符串消息
            ([&](const std::string& msg) {
                printf("Received: %s\n", msg.c_str());
            }),
            // 处理整数消息
            ([&](int value) {
                printf("Received integer: %d\n", value);
            })
        };
    }
};

void main() {
    // 创建 Actor 系统
    actor_system_config cfg;
    actor_system system(cfg);

    // 创建 Actor
    auto hello = system.spawn<HelloWorld>();

    // 发送消息 (tell 模式 - fire-and-forget)
    anon_send(hello, "Hello, CAF!");
    anon_send(hello, 42);

    // 使用 request/response 模式
    // auto response = request<hello>(std::string("ping")).await();
}

// 编译命令 (需要链接 caf_core):
// g++ -std=c++20 main.cpp -lcaf_core -o hello_caf
```

## CAF 核心概念

### 1. Actor 基类

```cpp
class MyActor : public event_based_actor {
    behavior make_behavior() override {
        return {
            ([&](message_handler& msg) {
                // 处理消息
            })
        };
    }
};
```

### 2. 消息发送

```cpp
// Tell (fire-and-forget)
self->send(target, message);

// Request/Response (带超时)
auto result = request(target, message).await(std::chrono::seconds(5));
```

### 3. Actor 监督

```cpp
// 创建带监督的 Actor
auto child = spawn<MyActor>([=] {
    return {}; // 初始化
}, self->link());  // 监督子 actor
```

### 4. 分布式 Actor

```cpp
// 连接远程节点
auto remote_node = system.middle().connect(hostname, port);

// 在远程节点上创建 Actor
auto remote_actor = system.spawn<RemoteActor>(remote_node, "actor_name");
```

## 迁移指南

### 从 Apollo Actor 迁移到 CAF

| Apollo Actor | CAF | 说明 |
|--------------|-----|------|
| `apollo::actor::Actor` | `caf::event_based_actor` | 基类 |
| `apollo::actor::ActorSystem` | `caf::actor_system` | 系统管理 |
| `apollo::actor::ActorRef` | `caf::actor` | Actor 引用 |
| `actor.tell(msg)` | `anon_send(actor, msg)` | 发送消息 |
| `actor.ask(msg)` | `request(actor, msg)` | 请求-响应 |

### 代码对比

**Apollo Actor (已删除)**:
```cpp
// 旧代码（不再可用）
#include "apollo/actor/actor.h"

class MyActor : public apollo::actor::Actor {
    void receive(const Message& msg) override {
        // ...
    }
};
```

**CAF (推荐)**:
```cpp
// 新代码
#include <caf/event_based_actor.hpp>

class MyActor : public caf::event_based_actor {
    behavior make_behavior() override {
        return {
            [&](const std::string& msg) {
                // 处理字符串消息
            }
        };
    }
};
```

## 参考资源

- [CAF 官方文档](https://actor-framework.readthedocs.io/)
- [CAF GitHub](https://github.com/actor-framework/actor-framework)
- [CAF 示例代码](https://github.com/actor-framework/actor-framework/tree/master/examples)
