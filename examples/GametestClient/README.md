# GametestClient

基于 Apollo 框架的游戏测试客户端，支持命令行交互和自动登录功能。

## 功能特性

- 🖥️ 交互式 Shell 界面，支持命令历史和自动补全
- 🔐 支持用户认证和自动登录
- 📡 实时接收和显示服务器消息（JSON 格式）
- 💾 登录配置保存和加载
- 🔧 可扩展的命令系统

## 依赖项

- **C++17** 或更高版本
- **nlohmann-json** (通过 vcpkg 集成)
- **readline** (Linux/macOS) / **WS2_32** (Windows)

## 构建方法

### 方法一：使用 Visual Studio 2022（推荐）

#### 前置条件
1. 安装 Visual Studio 2022（确保安装 C++ 开发工具）
2. 安装并配置 vcpkg

#### 配置 vcpkg

1. **安装 vcpkg**（如果还没有安装）
```bash
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
```

2. **集成到 Visual Studio**
```bash
.\vcpkg integrate install
```

3. **安装项目依赖**
```bash
# 在 Apollo 项目根目录
vcpkg install
```

#### 使用 Visual Studio 构建

1. **打开解决方案**
   - 双击 `Apollo.sln` 打开解决方案
   - Visual Studio 会自动检测并使用 vcpkg 集成

2. **选择配置**
   - 配置：Debug 或 Release
   - 平台：x64（推荐）或 Win32

3. **构建项目**
   - 右键点击 GametestClient 项目 → 生成
   - 或使用快捷键 `Ctrl+Shift+B`

4. **运行程序**
   - 设置 GametestClient 为启动项目
   - 按 `F5` 运行（带调试）
   - 或按 `Ctrl+F5` 运行（不调试）

### 方法二：使用 CMake（命令行）

#### 前置条件
```bash
# 安装 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
./bootstrap-vcpkg.sh  # Linux/macOS
# 或
./bootstrap-vcpkg.bat  # Windows

# 安装依赖
./vcpkg install nlohmann-json
```

#### 构建项目

```bash
# 创建构建目录
cd GametestClient
mkdir build && cd build

# 配置 CMake (指定 vcpkg 工具链)
cmake .. -DCMAKE_TOOLCHAIN_FILE=[vcpkg-root]/scripts/buildsystems/vcpkg.cmake

# 编译
cmake --build . --config Release
```

## 使用方法

### 1. 交互模式

```bash
# 运行客户端
./bin/gametest-client
```

进入交互模式后，可以使用以下命令：

- `help` - 显示帮助信息
- `connect <host> <port>` - 连接到服务器
- `login <username> <password>` - 用户登录
- `autologin [config_file]` - 使用配置文件自动登录
- `send <json_message>` - 发送 JSON 消息
- `status` - 显示当前连接状态
- `info` - 显示服务器信息
- `clear` - 清屏
- `quit/exit` - 退出程序

### 2. 自动登录模式

```bash
# 使用默认配置文件 (config/login.json)
./bin/gametest-client

# 使用自定义配置文件
./bin/gametest-client /path/to/config.json
```

### 配置文件格式

```json
{
    "host": "localhost",
    "port": 8080,
    "username": "your_username",
    "password": "your_password"
}
```

## 使用示例

### 交互式连接和登录

```bash
gametest> connect localhost 8080
正在连接到 localhost:8080...
连接成功!

gametest> login myusername mypassword
正在登录...
登录成功!

gametest> send {"type": "chat", "message": "Hello Server!"}
消息已发送: {
  "type": "chat",
  "message": "Hello Server!"
}

gametest> status
当前状态: 已认证

gametest> info
服务器信息:
{
  "type": "server_info",
  "version": "1.0.0",
  "online_users": 42
}

gametest> quit
正在退出...
```

### 发送 JSON 消息

```bash
# 发送简单消息
gametest> send {"type": "ping"}

# 发送带引号的字符串
gametest> send {"type": "chat", "message": "Hello World"}

# 发送复杂对象
gametest> send {"type": "action", "data": {"action": "move", "x": 100, "y": 200}}
```

## 扩展开发

### 添加新命令

在 `shell.cpp` 中注册新命令：

```cpp
registerCommand({
    "newcommand",
    "新命令描述",
    [this](const std::vector<std::string>& args) { cmdNewCommand(args); },
    "newcommand <arg1> <arg2>"
});
```

### 自定义消息处理

在 `main.cpp` 中设置消息处理器：

```cpp
void messageHandler(const json& message) {
    // 自定义处理逻辑
    if (message["type"] == "custom_event") {
        // 处理自定义事件
    }
}
```

## 项目结构

```
GametestClient/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── gametest_client.cpp   # 客户端核心实现
│   └── shell.cpp             # Shell 交互实现
├── include/
│   ├── gametest_client.h     # 客户端头文件
│   └── shell.h               # Shell 头文件
├── config/
│   └── login.json            # 示例配置文件
├── CMakeLists.txt            # CMake 构建文件
└── README.md                 # 项目说明
```

## 注意事项

1. 确保 vcpkg 已正确安装并配置了 nlohmann-json
2. Windows 系统会自动链接 WS2_32 库
3. Linux/macOS 系统需要安装 readline 开发包
4. 服务器接收到的所有消息都会以 JSON 格式打印到日志

## 许可证

本项目遵循 Apache 2.0 许可证。