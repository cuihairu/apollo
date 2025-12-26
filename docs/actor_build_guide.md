# Apollo Actor 框架 - 构建和使用指南

## 目录结构

```
apollo/
├── include/apollo/actor/          # Actor 框架头文件
│   ├── message.h                   # 消息定义和序列化接口
│   ├── actor_ref.h                 # Actor 引用（包含 ActorPath）
│   ├── actor.h                     # Actor 基类
│   ├── actor_cell.h                # Actor 执行单元
│   ├── actor_system.h              # Actor 系统
│   ├── actor_manager.h             # Actor 管理器
│   ├── dispatcher.h                # 消息调度器
│   ├── message_bus.h               # 消息总线
│   ├── timer_service.h             # 定时器服务
│   └── monitoring.h                # 监控和可观测性
│
├── src/apollo/actor/               # Actor 框架实现
│   ├── actor_cell.cpp
│   ├── actor_system.cpp
│   ├── actor_manager.cpp
│   ├── dispatcher.cpp
│   ├── message_bus.cpp
│   ├── monitoring.cpp
│   ├── timer_service.cpp
│   ├── actor_ref.cpp
│   └── message_serialization.cpp
│
├── tests/                          # 测试
│   ├── test_actor_framework.cpp
│   ├── test_actor_concurrency.cpp
│   └── README.md
│
├── examples/                       # 示例
│   ├── actor_demo.cpp
│   └── actor_advanced_demo.cpp
│
└── docs/                           # 文档
    └── actor_complete_guide.md
```

## 头文件依赖关系

```
┌─────────────────────────────────────────────────────────────────┐
│                          actor_ref.h                            │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ ActorPath + ActorRef + ActorRefSet                          │ │
│  │ 依赖: message.h                                             │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │
┌─────────────────────────────────────────────────────────────────┐
│                          actor.h                                │
│  ┌────────────────────────────────────────────────────────────┐ │
│  │ Actor + ActorContext + 内建消息                            │ │
│  │ 依赖: actor_ref.h, message.h                               │ │
│  └────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────┘
                              ▲
                              │
        ┌───────────────────────┼───────────────────────┐
        │                       │                       │
┌───────────────┐     ┌─────────────────┐    ┌──────────────┐
│ actor_cell.h  │     │ actor_system.h  │    │actor_manager.│
│ + Envelope    │     │ + ActorSystem   │    │h            │
└───────────────┘     └─────────────────┘    └──────────────┘
        ▲                       ▲                       ▲
        │                       │                       │
┌───────────────┐     ┌─────────────────┐    ┌──────────────┐
│ dispatcher.h  │     │ message_bus.h   │    │timer_service.│
└───────────────┘     └─────────────────┘    │h             │
                                               └──────────────┘
```

## 编译配置

### CMake 最低要求

```cmake
cmake_minimum_required(VERSION 3.16)
```

### C++ 标准

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

## 编译步骤

### Linux/macOS

```bash
# 1. 配置
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DAPOLLO_BUILD_TESTS=ON

# 2. 编译
cmake --build . -j$(nproc)

# 3. 运行测试
ctest --output-on-failure

# 4. 运行示例
./examples/actor_demo
./examples/actor_advanced_demo
```

### Windows (Visual Studio)

```cmd
REM 1. 配置
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64

REM 2. 编译
cmake --build . --config Release

REM 3. 运行测试
ctest -C Release --output-on-failure
```

### Windows (MinGW)

```bash
# 1. 配置
mkdir build && cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release

# 2. 编译
cmake --build . -j

# 3. 运行
./examples/actor_demo.exe
```

## 可选依赖

### FlatBuffers（推荐）

```bash
# Ubuntu/Debian
sudo apt-get install libflatbuffers-dev

# macOS (Homebrew)
brew install flatbuffers

# vcpkg
vcpkg install flatbuffers
```

### YAML 支持

```bash
# Ubuntu/Debian
sudo apt-get install libyaml-cpp-dev

# macOS (Homebrew)
brew install yaml-cpp

# vcpkg
vcpkg install yaml-cpp
```

### SQLite（服务发现）

```bash
# Ubuntu/Debian
sudo apt-get install libsqlite3-dev

# macOS
brew install sqlite3

# Windows (vcpkg)
vcpkg install sqlite3
```

## 编译选项

| 选项 | 默认值 | 说明 |
|------|--------|------|
| `APOLLO_BUILD_TESTS` | `ON` | 构建测试 |
| `APOLLO_BUILD_EXAMPLES` | `ON` | 构建示例 |
| `APOLLO_ENABLE_YAML` | `ON` | 启用 YAML 支持 |
| `APOLLO_ENABLE_COVERAGE` | `OFF` | 启用代码覆盖率 |

## 常见编译问题

### 1. 找不到 protobuf

```bash
# 使用 vcpkg
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg]/scripts/buildsystems/vcpkg.cmake
```

### 2. C++20 不可用

```bash
# 升级编译器
# GCC >= 10, Clang >= 12, MSVC >= 2019
```

### 3. 链接错误

```bash
# 确保链接了正确的库
target_link_libraries(your_target PRIVATE apollo pthread)
```

## 运行时配置

### 环境变量

| 变量 | 说明 | 默认值 |
|------|------|--------|
| `APOLLO_SYSTEM_NAME` | Actor 系统名称 | `apollo` |
| `APOLLO_ACTOR_THREADS` | 调度线程数 | CPU 核心数 |
| `APOLLO_IO_THREADS` | IO 线程数 | 2 |
| `APOLLO_MAILBOX_SIZE` | 邮箱大小 | 256 |

### 配置文件

```yaml
# actor-config.yaml
system:
  name: "my-game-server"
  address: "127.0.0.1"
  dataCenter: "shanghai"
  hostId: "server-001"

threads:
  dispatcher: 4
  io: 2
  background: 2

mailbox:
  capacity: 256
  highWatermark: 192
  lowWatermark: 64

discovery:
  backend: "sqlite"  # sqlite, redis, memory
  dbPath: ":memory:"

monitoring:
  enableHttpApi: true
  httpApiPort: 8080
  enablePrometheus: false
```

## 性能调优

### 1. 线程池配置

```cpp
ThreadPoolConfig config;
config.dispatcherThreads = std::thread::hardware_concurrency();
config.ioThreads = 4;
config.dispatchStrategy = DispatchStrategy::LeastLoaded;
```

### 2. 邮箱大小

```cpp
MailboxConfig mailboxConfig;
mailboxConfig.capacity = 1024;  // 增加容量
mailboxConfig.strategy = ipc::BackpressureStrategy::Block;
```

### 3. 消息序列化

```cpp
// 使用 Binary 格式（最快）
Message msg(data, MessageFormat::Binary);

// 使用 FlatBuffers（零拷贝）
Message msg(data, MessageFormat::FlatBuffers);
```

### 4. 本地通信优先

```cpp
// 自动选择最优传输
endpoint.transportMask = TransportMask::SharedMemory |
                         TransportMask::UnixSocket |
                         TransportMask::Tcp;
```

## 调试

### 启用调试日志

```cpp
#define APOLLO_DEBUG_LOG 1
```

### 内存检测

```bash
# Valgrind
valgrind --leak-check=full --show-leak-kinds=all ./your_app

# AddressSanitizer
cmake .. -DSANITIZE_ADDRESS=ON
cmake --build .
./your_app

# ThreadSanitizer
cmake .. -DSANITIZE_THREAD=ON
cmake --build .
./your_app
```

### 性能分析

```bash
# perf (Linux)
perf record -g ./your_app
perf report

# gprof
cmake .. -DCMAKE_BUILD_TYPE=Debug -DPROFILE=ON
cmake --build .
./your_app
gprof your_app gmon.out > analysis.txt
```

## 部署

### Docker

```dockerfile
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    g++ \
    cmake \
    libprotobuf-dev \
    libyaml-cpp-dev \
    libsqlite3-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .
RUN mkdir build && cd build && \
    cmake .. -DCMAKE_BUILD_TYPE=Release && \
    cmake --build . -j$(nproc)

CMD ["./build/examples/actor_advanced_demo"]
```

### 系统服务

```ini
# /etc/systemd/system/apollo-game-server.service
[Unit]
Description=Apollo Game Server
After=network.target

[Service]
Type=simple
User=apollo
WorkingDirectory=/opt/apollo
ExecStart=/opt/apollo/bin/game-server --config /etc/apollo/config.yaml
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

## 故障排查

### 问题：Actor 消息丢失

1. 检查邮箱水位线
2. 检查背压策略
3. 启用调试日志

### 问题：性能下降

1. 使用 profiler 分析瓶颈
2. 调整线程池大小
3. 检查序列化格式

### 问题：死锁

1. 使用 ThreadSanitizer 检测
2. 检查 ask() 模式的超时设置
3. 避免 Actor 循环等待
