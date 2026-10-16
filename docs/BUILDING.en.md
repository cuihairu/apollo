[English](BUILDING.en.md) | [中文](BUILDING.md)

# Building the Apollo Server

## Using vcpkg (recommended)

### 1. Install vcpkg

```bash
# Clone vcpkg
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg

# Linux/macOS
./bootstrap-vcpkg.sh

# Windows
.\bootstrap-vcpkg.bat
```

### 2. Build the project

```bash
# Return to the project root
cd ..

# Configure CMake
cmake -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DVCPKG_TARGET_TRIPLET=x64-linux # Linux

# Build
cmake --build build --config Release
```

## Using System Package Managers

### Ubuntu/Debian

```bash
# Install dependencies
sudo apt-get install nlohmann-json3-dev libprotobuf-dev protobuf-compiler libgtest-dev libssl-dev libcurl4-openssl-dev libpugixml-dev libreadline-dev

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS

```bash
# Using Homebrew
brew install nlohmann-json protobuf gtest openssl curl pugixml readline

# Build
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Windows

```bash
# Using vcpkg (recommended)
vcpkg install nlohmann-json protobuf gtest openssl curl pugixml readline crashpad:x64-windows

# Build
cmake -B build `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_TOOLCHAIN_FILE=./vcpkg/scripts/buildsystems/vcpkg.cmake `
  -DVCPKG_TARGET_TRIPLET=x64-windows

cmake --build build --config Release
```

## Build Options

### CMake options
- `APOLLO_BUILD_TESTS=ON` - build unit tests (default: ON)
- `APOLLO_BUILD_EXAMPLES=ON` - build example programs (default: ON)
- `APOLLO_ENABLE_COVERAGE=ON` - enable code coverage (requires gcov/lcov)
- `APOLLO_ENABLE_DOXYGEN=ON` - generate API documentation (requires Doxygen)
- `APOLLO_ENABLE_CLANG_TIDY=ON` - enable clang-tidy static analysis

### vcpkg dependencies

The vcpkg.json manifest mode installs the following dependencies automatically (no manual vcpkg install needed):

| Dependency | Purpose |
|------|------|
| openssl | TLS/encryption |
| protobuf | Serialization (golden .proto) |
| gtest | Unit tests |
| curl | HTTP client |
| nlohmann-json | JSON parsing |
| pugixml | XML parsing (contracts) |
| readline | Interactive command line |
| crashpad | Crash capture |

## Running Tests

```bash
cd build
ctest --output-on-failure -C Release
```

## Generating Documentation

```bash
cmake -B build -DAPOLLO_ENABLE_DOXYGEN=ON
cd build
make doc

# Documentation will be generated in build/docs/html/
```

## Developer Guide

### Adding a New Dependency

1. Edit `vcpkg.json` to add the dependency:
```json
{
  "dependencies": [
    "nlohmann-json",
    "your-new-dependency"
  ]
}
```

2. Update `CMakeLists.txt`:
```cmake
find_package(your-dependency CONFIG REQUIRED)
target_link_libraries(apollo PRIVATE your-dependency::your-dependency)
```

3. Install the new dependency:
```bash
cd vcpkg
./vcpkg install your-dependency
```

### Windows Development Tips

- Use Visual Studio 2019 or later
- Install the CMake tools
- Use the x64 Native Tools Command Prompt
- Consider using CLion for development

### Linux Development Tips

- CLion or VSCode + the CMake plugin recommended
- Install clang-tidy for static analysis
- Use valgrind for memory checking

## Troubleshooting

### Dependencies Not Found

If CMake cannot find a dependency, ensure:

1. vcpkg has installed the dependencies correctly
2. The `CMAKE_TOOLCHAIN_FILE` path is correct
3. The correct `VCPKG_TARGET_TRIPLET` is used

### Windows-Specific Issues

1. Ensure Python is installed (vcpkg needs it)
2. Use the x64 toolchain
3. You may need to install Visual Studio Build Tools

### macOS-Specific Issues

1. Update Xcode to the latest version
2. Install the Xcode Command Line Tools

## Performance Optimization Tips

1. Use the Release build for performance testing
2. Enable compiler optimizations: `-O3 -march=native`
3. Use LTO (link-time optimization): `-DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON`
4. Use static linking to reduce dependencies: `-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded`
