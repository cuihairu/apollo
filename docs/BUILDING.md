# 构建 Apollo MMORPG 框架

## 使用 vcpkg（推荐）

### 1. 安装 vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Linux/macOS
./bootstrap-vcpkg.sh

# Windows
.\bootstrap-vcpkg.bat
```

### 2. 构建项目

```bash
# 返回到项目根目录
cd ..

# 配置 CMake
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux # Linux

# 构建
cmake --build build --config Release
```

## 使用系统包管理器

### Ubuntu/Debian

```bash
# 安装依赖
sudo apt-get install nlohmann-json3-dev libprotobuf-dev protobuf-compiler libgtest-dev libfmt-dev

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS

```bash
# 使用 Homebrew
brew install nlohmann-json protobuf gtest fmt

# 构建
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

```bash
# 使用 vcpkg（推荐）
vcpkg install nlohmann-json protobuf gtest fmt:x64-windows

# 构建
cmake -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Release
```

## 构建选项

### CMake 选项
- `APOLLO_BUILD_TESTS=ON` - 构建单元测试（默认：ON）
- `APOLLO_BUILD_EXAMPLES=ON` - 构建示例程序（默认：ON）
- `APOLLO_ENABLE_COVERAGE=ON` - 启用代码覆盖率（需要 gcov/lcov）
- `APOLLO_ENABLE_DOXYGEN=ON` - 生成 API 文档（需要 Doxygen）
- `APOLLO_ENABLE_CLANG_TIDY=ON` - 启用 clang-tidy 静态分析

### vcpkg Features
vcpkg.json 定义了以下可选功能：

```bash
# 安装所有依赖（默认）
vcpkg install --triplet=x64-linux

# 只安装基本依赖（不包括 Redis 和额外功能）
vcpkg install --triplet=x64-linux --x-feature=basic

# 安装包含所有功能的依赖
vcpkg install --triplet=x64-linux --x-feature=full

# 或者在 vcpkg.json 中指定要安装的 features：
{
  "dependencies": [
    "apollo-mmorpg[tests,redis]"
  ]
}
```

可用的 features：
- `tests` - 包含 gtest 和 gmock
- `redis` - Redis 客户端支持
- `protobuf` - Protocol Buffers 支持（包含 zlib）

## 运行测试

```bash
cd build
ctest --output-on-failure -C Release
```

## 生成文档

```bash
cmake -B build -DAPOLLO_ENABLE_DOXYGEN=ON
cd build
make doc

# 文档将生成在 build/docs/html/
```

## 开发者指南

### 添加新的依赖

1. 编辑 `vcpkg.json` 添加依赖：
```json
{
  "dependencies": [
    "nlohmann-json",
    "your-new-dependency"
  ]
}
```

2. 更新 `CMakeLists.txt`：
```cmake
find_package(your-dependency CONFIG REQUIRED)
target_link_libraries(apollo PRIVATE your-dependency::your-dependency)
```

3. 安装新依赖：
```bash
cd vcpkg
./vcpkg install your-dependency
```

### Windows 开发提示

- 使用 Visual Studio 2019 或更新版本
- 安装 CMake 工具
- 使用 x64 Native Tools Command Prompt
- 考虑使用 CLion 进行开发

### Linux 开发提示

- 推荐使用 CLion 或 VSCode + CMake 插件
- 安装 clang-tidy 用于静态分析
- 使用 valgrind 进行内存检查

## 故障排除

### 找不到依赖

如果 CMake 找不到依赖，请确保：

1. vcpkg 已正确安装依赖
2. `CMAKE_TOOLCHAIN_FILE` 路径正确
3. 使用正确的 `VCPKG_TARGET_TRIPLET`

### Windows 特定问题

1. 确保 Python 已安装（vcpkg 需要）
2. 使用 x64 工具链
3. 可能需要安装 Visual Studio Build Tools

### macOS 特定问题

1. 更新 Xcode 到最新版本
2. 安装 Xcode Command Line Tools

## 性能优化建议

1. 使用 Release 构建进行性能测试
2. 启用编译器优化：`-O3 -march=native`
3. 使用 LTO（链接时优化）：`-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`
4. 使用静态链接以减少依赖：`-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`