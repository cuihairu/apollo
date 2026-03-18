---
title: Actor 模块
icon: actor
order: 6
category:
  - 模块
tag:
  - actor
  - 并发
---

# Actor 模块

Actor 模块提供基于 Actor 的并发模型，每个 Actor 有独立的消息队列。

## Actor 基础

```cpp
#include <apollo/actor/actor.hpp>

using namespace apollo::actor;

// 定义 Actor
class PlayerActor : public Actor {
protected:
    void onStart() override {
        LOG_INFO("PlayerActor", "Actor 启动");
    }

    void onMessage(const Message& msg) override {
        if (msg.type == "Move") {
            auto pos = msg.data<Position>();
            handleMove(pos);
        } else if (msg.type == "Attack") {
            auto target = msg.data<int64_t>();
            handleAttack(target);
        }
    }

    void onStop() override {
        LOG_INFO("PlayerActor", "Actor 停止");
    }

private:
    void handleMove(const Position& pos) {
        position_ = pos;
        LOG_INFO("PlayerActor", "移动到 ({}, {})", pos.x, pos.y);
    }

    void handleAttack(int64_t targetId) {
        LOG_INFO("PlayerActor", "攻击目标 {}", targetId);
    }

    Position position_;
};

// 使用 Actor
auto actor = ActorSystem::instance().spawn<PlayerActor>();

// 发送消息
actor->send(Message("Move", Position{100, 200}));
actor->send(Message("Attack", (int64_t)12345));
```

## Actor 通信

### Tell 发送

```cpp
// 发送消息（不等待响应）
actor1->tell(actor2, Message("Hello", "数据"));
```

### Ask 请求

```cpp
// 发送消息并等待响应
auto future = actor1->ask(actor2, Message("GetData"));
auto response = future.get();  // 阻塞等待
```

### 广播

```cpp
// 向多个 Actor 广播
ActorSystem::instance().broadcast(Message("Shutdown"));
```

## Actor 生命周期

```cpp
class MyActor : public Actor {
protected:
    void onStart() override {
        // Actor 启动时调用
        // 初始化资源
    }

    void onMessage(const Message& msg) override {
        // 处理消息
    }

    void onStop() override {
        // Actor 停止时调用
        // 清理资源
    }
};
```

## Actor 监督

```cpp
// 创建监督策略
auto strategy = SupervisorStrategy::oneForOne();

// 创建被监督的 Actor
auto child = ActorSystem::instance().spawn<ChildActor>();

// 创建监督者
auto supervisor = ActorSystem::instance()
    .supervisor(strategy)
    .spawn<SupervisorActor>();

supervisor->watch(child, [](const ActorError& error) {
    LOG_ERROR("Supervisor", "子 Actor 出错: {}", error.what());
    // 重启子 Actor
    error.actor->restart();
});
```

## Actor 定时器

```cpp
class MyActor : public Actor {
protected:
    void onStart() override {
        // 单次定时器
        setTimeout(1000, [this]() {
            LOG_INFO("Actor", "1秒后执行");
        });

        // 周期定时器
        setInterval(5000, [this]() {
            LOG_INFO("Actor", "每5秒执行");
        });
    }
};
```

## 分布式 Actor

```cpp
// 远程 Actor
auto remoteActor = ActorSystem::instance()
    .remote("tcp://192.168.1.100:8888")
    .lookup("PlayerActor");

// 发送消息到远程 Actor
remoteActor->send(Message("Move", Position{100, 200}));
```

## 依赖

- apollo::runtime
- apollo::net

## 链接

```cmake
find_package(apollo-actor REQUIRED)
target_link_libraries(my_app apollo::actor)
```
