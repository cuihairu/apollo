---
title: Installation
icon: download
order: 2
prev: /en/guide/
next: /en/guide/quick-start
---

# Install Apollo

## Requirements

- **OS**: Windows 10+, Linux (Ubuntu 20.04+), macOS 11+
- **Compilers**:
  - Windows: MSVC 2019+ (Visual Studio 2019+)
  - Linux: GCC 10+ or Clang 12+
  - macOS: Xcode 13+ (Clang 13+)
- **CMake**: 3.20+
- **vcpkg**: 2023.0+ (for dependency management)

## Installation steps

### 1. Install vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/microsoft/vcpkg.git C:/vcpkg
cd C:/vcpkg

# 运行 bootstrap
./bootstrap-vcpkg.bat  # Windows
# 或
./bootstrap-vcpkg.sh   # Linux/macOS
```

### 2. Install dependencies

```bash
# 仓库采用 vcpkg 清单模式（vcpkg.json），CMake configure 期自动安装全部依赖
#（openssl / protobuf / gtest / curl / nlohmann-json / pugixml，Linux 另含 readline / crashpad），
# 无需手工逐个安装：
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=<vcpkg-root>/scripts/buildsystems/vcpkg.cmake
```

### 3. Clone Apollo

```bash
git clone https://github.com/your-org/apollo.git
cd apollo
```

### 4. Configure and build

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

### 5. Verify the installation

```bash
# 运行测试
ctest --test-dir build --output-on-failure

# 运行示例（world 入口自检）
./build/apps/world/world --help
```

## Directory structure

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

## Next steps

Once installed, continue with [Quick Start](./quick-start.md).
