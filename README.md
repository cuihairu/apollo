# Apollo IoC Framework

一个轻量级C++ IoC（控制反转）框架，提供组件生命周期管理、依赖注入和配置管理功能。

## 特性

- 🏗️ **轻量级设计** - 零依赖（除可选的nlohmann/json），纯头文件实现
- 🔄 **完整的生命周期管理** - Initialize、Start、Stop、Destroy
- 🧩 **自动依赖解析** - 拓扑排序算法自动解析依赖关系
- ⚙️ **类型安全的配置系统** - 支持自动重载和变更通知
- 📝 **自动组件注册** - 使用宏实现零配置注册
- 🔍 **依赖循环检测** - 自动检测并报告循环依赖
- 📁 **文件监控支持** - 自动重载配置文件变更
- 🎯 **线程安全** - 支持多线程环境

## 快速开始

### 基本使用

```cpp
#include "common/BaseComponent.h"
#include "common/ComponentRegistry.h"
#include "common/ApplicationContext.h"

using namespace Apollo;

// 定义组件
class MyService : public BaseComponent {
public:
    MyService() : BaseComponent("MyService") {}

    DECLARE_COMPONENT(MyService, "MyService")

    bool onInitialize() override {
        std::cout << "Service initialized!" << std::endl;
        return true;
    }
};

// 自动注册组件
REGISTER_COMPONENT(MyService)

int main() {
    auto& context = ApplicationContext::getInstance();

    // 初始化并启动所有组件
    context.initializeComponents();
    context.startComponents();

    // 使用组件
    auto service = context.getComponent<MyService>();

    // 清理
    context.destroyComponents();
    return 0;
}
```

### 依赖注入

```cpp
class UserService : public BaseComponent {
public:
    UserService() : BaseComponent("UserService") {
        // 声明依赖
        addDependency("DatabaseService");
        addDependency("LoggingService");
    }

    bool onInitialize() override {
        // 自动获取依赖
        auto db = getComponent<DatabaseService>();
        auto logger = getComponent<LoggingService>();
        // ...
    }
};
```

### 配置管理

```cpp
class WebServer : public BaseComponent {
private:
    // 类型安全的配置属性
    CONFIG_PROPERTY(int, port, "server.port", 8080);
    CONFIG_PROPERTY(bool, enableSsl, "server.ssl", false);

public:
    bool onInitialize() override {
        std::cout << "Starting on port " << port << std::endl;

        // 监听配置变更
        port.addChangeListener([this](int newPort) {
            std::cout << "Port changed to " << newPort << std::endl;
        });

        return true;
    }
};
```

## 编译要求

- C++17 或更高版本
- CMake 3.16+
- 可选：nlohmann/json（用于JSON配置支持）

## 构建安装

```bash
git clone <repository>
cd apollo
mkdir build && cd build

# 基本构建
cmake ..

# 启用示例
cmake -DBUILD_EXAMPLES=ON ..

# 启用测试
cmake -DBUILD_TESTS=ON ..

# 构建
make -j$(nproc)

# 安装
sudo make install
```

## 核心组件

### 1. IComponent / BaseComponent
- 定义组件生命周期接口
- 提供线程安全的状态管理
- 支持依赖关系声明

### 2. ApplicationContext
- 核心IoC容器
- 单例模式管理
- 自动组件注册和检索

### 3. DependencyManager
- 拓扑排序算法
- 循环依赖检测
- 依赖关系验证

### 4. ConfigManager
- 类型安全的配置管理
- 支持JSON格式
- 自动重载功能

### 5. FileWatcher
- 跨平台文件监控
- 事件驱动通知
- 高效的轮询机制

## 最佳实践

1. **组件设计**：
   - 保持组件单一职责
   - 明确声明依赖关系
   - 避免循环依赖

2. **配置管理**：
   - 使用类型安全的CONFIG_PROPERTY
   - 设置合理的默认值
   - 监听重要配置的变更

3. **生命周期**：
   - 在onInitialize中完成初始化
   - 在onStart中启动服务
   - 在onStop中清理资源

## 示例程序

查看 `examples/` 目录中的示例：

- `basic_example.cpp` - 基本用法演示
- `config_example.cpp` - 配置管理演示
- `dependency_example.cpp` - 依赖管理演示

## 许可证

MIT License