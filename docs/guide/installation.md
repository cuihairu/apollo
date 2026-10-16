---
title: 安装
icon: download
order: 2
prev: /guide/
next: /guide/quick-start
---

[English](installation.en.md) | [中文](installation.md)

# 安装 Apollo

## 环境要求

- **操作系统**: Windows 10+, Linux (Ubuntu 20.04+), macOS 11+
- **编译器**:
  - Windows: MSVC 2019+ (Visual Studio 2019+)
  - Linux: GCC 10+ 或 Clang 12+
  - macOS: Xcode 13+ (Clang 13+)
- **CMake**: 3.20+
- **vcpkg**: 2023.0+ (用于依赖管理)

## 安装步骤

### 1. 安装 vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg

# 运行 bootstrap
./bootstrap-vcpkg.bat  # Windows
# 或
./bootstrap-vcpkg.sh   # Linux/macOS
```

### 2. 安装依赖

```bash
# 仓库采用 vcpkg 清单模式（vcpkg.json），CMake configure 期自动安装全部依赖
#（openssl / protobuf / gtest / curl / nlohmann-json / pugixml，Linux 另含 readline / crashpad），
# 无需手工逐个安装：
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

### 3. 克隆 Apollo

```bash
git clone https://github.com/your-org/apollo.git
cd apollo
```

### 4. 配置与构建

#### Windows

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release ^
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake ^
  -DAPOLLO_ENABLE_MODULAR_LAYOUT=ON

cmake --build build --config Release
```

#### Linux/macOS

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DAPOLLO_ENABLE_MODULAR_LAYOUT=ON

cmake --build build -j$(nproc)
```

### 5. 验证安装

```bash
# 运行测试
ctest --test-dir build --output-on-failure

# 运行示例
./build/apps/game-server/apollo_game_server
```

## 目录结构

```
apollo/
├── apps/              # 可执行程序
├── modules/           # 模块源码
│   ├── base/         # 基础模块
│   ├── core/         # 核心模块
│   ├── runtime/      # 运行时模块
│   ├── data/         # 数据访问模块
│   ├── net/          # 网络模块
│   ├── protocol/     # 协议模块
│   ├── contract/     # 契约模块
│   ├── game/         # 游戏模块
│   └── bigworld/     # BigWorld 兼容层
├── include/          # 公共头文件
├── tests/            # 测试代码
└── docs/             # 文档
```

## 下一步

安装完成后，继续阅读 [快速开始](./quick-start.md)。
