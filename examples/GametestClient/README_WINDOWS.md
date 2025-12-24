# Windows 平台使用说明

## 📋 前置条件

在 Windows 平台上，需要安装 readline 兼容库以支持自动补全功能。

## 🔧 安装方法

### 方法一：使用 vcpkg 安装 readline（推荐）

```bash
# 安装 readline for Windows
vcpkg install readline:x64-windows

# 或者使用静态链接版本
vcpkg install readline:x64-windows-static
```

### 方法二：使用预编译的 readline 库

1. 下载 [GnuWin32](http://gnuwin32.sourceforge.net/packages/readline.htm) 的 readline 包
2. 将头文件和库文件添加到项目路径

### 方法三：使用 msys2

```bash
# 在 msys2 环境中
pacman -S mingw-w64-x86_64-readline
```

## 🎯 功能特性

当前 Shell 实现包含以下高级功能：

### 1. **Tab 自动补全**
   - 命令名称自动补全
   - 参数智能补全（主机名、端口号、配置文件等）
   - 历史命令补全
   - 不区分大小写匹配

### 2. **命令历史记录**
   - 上下箭头浏览历史命令
   - 历史记录持久化保存（~/.gametest_history）
   - 最多保存 1000 条历史
   - 自动去除重复命令

### 3. **智能提示**
   - 命令错误时提供相似命令建议
   - 使用 Levenshtein 距离算法计算相似度
   - 参数数量错误提示
   - JSON 格式错误提示

### 4. **彩色输出**
   - 状态显示带颜色（Linux/macOS）
   - 错误信息突出显示
   - 成功信息友好提示

## 📝 使用示例

### Tab 补全演示

```bash
gametest> con<Tab>          # 自动补全为 "connect"
gametest> connect local<Tab> # 自动补全为 "localhost"
gametest> connect localhost <Tab> # 显示可用端口号

gametest> au<Tab>           # 自动补全为 "autologin"
gametest> autologin <Tab>   # 显示可用配置文件
```

### 智能提示演示

```bash
gametest> conect localhost 8080  # 拼写错误
未知命令: conect
您是否想输入: connect ?        # 智能建议

gametest> connect              # 参数不足
用法: connect <host> <port>
提示: 使用 Tab 键可以自动补全主机名和端口号
```

### 历史记录使用

```bash
# 使用上下箭头浏览历史命令
gametest> ↑ (上箭头)  # 显示上一条命令
gametest> ↓ (下箭头)  # 显示下一条命令

# 搜索历史命令（Ctrl+R）
```

## 🚀 编译注意事项

### Visual Studio 项目配置

项目文件已自动配置好：
- 包含路径：`$(VcpkgRootDir)\installed\$(VCPKG_TARGET_TRIPLET)\include`
- 库路径：`$(VcpkgRootDir)\installed\$(VCPKG_TARGET_TRIPLET)\lib`
- 链接库：`readline.lib`

### CMake 配置

```cmake
# 在 Windows 上查找 readline
if(WIN32)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(READLINE REQUIRED readline)
    target_link_libraries(gametest-client PRIVATE ${READLINE_LIBRARIES})
    target_include_directories(gametest-client PRIVATE ${READLINE_INCLUDE_DIRS})
endif()
```

## 🔍 故障排除

### 1. readline 相关错误

**错误**: `fatal error: readline/readline.h: No such file or directory`
**解决**: 确保已安装 readline 并正确配置包含路径

**错误**: `unresolved external symbol _readline`
**解决**: 确保链接了 readline 库文件

### 2. vcpkg 集成问题

```bash
# 重新集成 vcpkg 到 Visual Studio
vcpkg integrate install

# 重新安装 readline
vcpkg reinstall readline:x64-windows
```

### 3. 替代方案

如果无法使用 readline，可以使用 Windows API 实现简化版本：

```cpp
// 简单的输入获取（无自动补全）
std::string getLine(const std::string& prompt) {
    std::cout << prompt;
    std::string line;
    std::getline(std::cin, line);
    return line;
}
```

## 💡 最佳实践

1. **使用 vcpkg 管理依赖**：简化库的管理和版本控制
2. **启用 IntelliSense**：确保 Visual Studio 正确识别头文件
3. **使用 x64 平台**：现代 Windows 系统推荐使用 64 位
4. **配置调试输出**：在开发时启用详细的调试信息

## 📚 参考资源

- [GNU Readline Manual](https://tiswww.case.edu/php/chet/readline/rltop.html)
- [vcpkg 文档](https://vcpkg.readthedocs.io/)
- [Visual Studio 集成 vcpkg](https://docs.microsoft.com/en-us/cpp/build/vcpkg)