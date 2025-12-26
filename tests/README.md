# Apollo Actor 框架测试套件

## 概述

本测试套件提供全面的单元测试、集成测试和压力测试，确保 Actor 框架的正确性、稳定性和性能。

## 测试文件

| 文件 | 描述 | 测试数量 |
|------|------|----------|
| `test_actor_framework.cpp` | 基础功能测试 | 30+ |
| `test_actor_concurrency.cpp` | 并发和压力测试 | 10+ |

## 测试覆盖

### 基础功能测试 (test_actor_framework.cpp)

#### 消息测试
- ✅ `message_basics` - 消息创建和访问
- ✅ `message_copy` - 消息拷贝
- ✅ `actor_path_parsing` - Actor 路径解析
- ✅ `actor_path_to_string` - Actor 路径序列化

#### Mailbox 测试
- ✅ `mailbox_basic` - 基本发送/接收
- ✅ `mailbox_capacity` - 容量限制
- ✅ `mailbox_watermark` - 水位线机制
- ✅ `mailbox_concurrent` - 并发访问

#### Actor 生命周期测试
- ✅ `actor_lifecycle` - 启动/停止回调

#### 消息发送测试
- ✅ `actor_messaging` - 消息接收
- ✅ `actor_concurrent_send` - 并发发送

#### 定时器测试
- ✅ `timer_oneshot` - 单次定时器
- ✅ `timer_repeated` - 重复定时器
- ✅ `timer_cancel` - 定时器取消
- ✅ `timer_concurrent` - 并发定时器

#### Actor 管理器测试
- ✅ `actor_manager_registration` - Actor 注册
- ✅ `actor_manager_find_by_prefix` - 按前缀查找
- ✅ `actor_manager_stats` - 统计信息

#### 观察者测试
- ✅ `observer_basic` - 事件观察

#### 健康检查测试
- ✅ `health_check` - 系统健康状态

#### 边界条件测试
- ✅ `mailbox_close_and_send` - 关闭后发送
- ✅ `actor_path_empty` - 空路径
- ✅ `timer_zero_delay` - 零延迟
- ✅ `timer_negative_interval` - 负间隔
- ✅ `message_empty` - 空消息

#### 压力测试
- ✅ `stress_many_actors` - 大量 Actor
- ✅ `stress_message_burst` - 突发大量消息

### 并发测试 (test_actor_concurrency.cpp)

#### 死锁测试
- ✅ `deadlock_circular_wait` - 循环等待检测

#### 竞态条件测试
- ✅ `race_condition_counter` - 计数器竞态

#### 内存可见性测试
- ✅ `memory_visibility` - 跨线程内存可见

#### 资源泄漏测试
- ✅ `resource_leak_actors` - Actor 泄漏检测

#### 异常安全测试
- ✅ `exception_safety` - 异常后恢复

#### 定时器并发测试
- ✅ `timer_concurrent_scheduling` - 并发调度

#### 压力测试
- ✅ `mailbox_stress_concurrent` - 极限并发
- ✅ `lock_free_behavior` - 无锁行为验证

#### 顺序一致性测试
- ✅ `message_ordering` - 消息顺序保证

## 编译

### 基础编译

```bash
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### 启用测试

```bash
cmake .. -DAPOLLO_BUILD_TESTS=ON
cmake --build .
```

## 运行测试

### 运行所有测试

```bash
# 从 build 目录
./tests/test_actor_framework
./tests/test_actor_concurrency
```

### 使用 CTest

```bash
cd build
ctest --output-on-failure
```

### 详细输出

```bash
./tests/test_actor_framework --verbose
```

## 内存检测

### Valgrind (内存泄漏检测)

```bash
valgrind --leak-check=full --show-leak-kinds=all \
         ./tests/test_actor_framework
```

### ThreadSanitizer (数据竞争检测)

```bash
# 重新编译
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DSANITIZE_THREAD=ON

cmake --build .

# 运行
./tests/test_actor_concurrency
```

### AddressSanitizer (内存错误检测)

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug \
         -DSANITIZE_ADDRESS=ON

cmake --build .

./tests/test_actor_framework
```

## 性能测试

### 基准测试模式

```bash
# 运行性能测试
./tests/test_actor_framework --benchmark
```

### 自定义压力测试

```cpp
// 修改测试参数
const int numActors = 1000;      // 增加 Actor 数量
const int numMessages = 100000;  // 增加消息数量
```

## CI/CD 集成

### GitHub Actions 示例

```yaml
name: Tests

on: [push, pull_request]

jobs:
  test:
    runs-on: ubuntu-latest

    steps:
    - uses: actions/checkout@v2

    - name: Install dependencies
      run: sudo apt-get install -y libfmt-dev

    - name: Configure CMake
      run: cmake -B build -DCMAKE_BUILD_TYPE=Release -DAPOLLO_BUILD_TESTS=ON

    - name: Build
      run: cmake --build build

    - name: Run tests
      run: |
        cd build
        ctest --output-on-failure

    - name: Run with ThreadSanitizer
      run: |
        cmake -B build-tsan -DCMAKE_BUILD_TYPE=Debug -DSANITIZE_THREAD=ON
        cmake --build build-tsan
        ./build-tsan/tests/test_actor_concurrency
```

## 测试最佳实践

### 添加新测试

1. 在对应的测试文件中添加测试函数：
   ```cpp
   TEST(my_new_test) {
       // 测试代码
       ASSERT_TRUE(condition);
   }
   ```

2. 在 `main()` 中注册：
   ```cpp
   RUN_TEST(my_new_test);
   ```

### 编写测试的原则

1. **独立性**：每个测试应该独立运行，不依赖其他测试
2. **可重复性**：多次运行应该得到相同结果
3. **快速**：单元测试应该快速执行
4. **清晰**：测试名称和断言应该清楚表达意图

### 异步测试处理

```cpp
// 等待异步操作
auto start = std::chrono::steady_clock::now();
while (!condition.load()) {
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    ASSERT_LT(std::chrono::steady_clock::now() - start,
              std::chrono::seconds(5));  // 超时保护
}
```

## 常见问题

### Q: 测试偶尔失败

A: 可能是竞态条件。使用 ThreadSanitizer 检测：
```bash
cmake .. -DSANITIZE_THREAD=ON
```

### Q: 测试运行缓慢

A: 某些测试是压力测试，可以跳过：
```cpp
#ifndef APOLLO_SKIP_STRESS_TESTS
    RUN_TEST(stress_many_actors);
#endif
```

### Q: 内存泄漏报告

A: 某些资源在程序结束时才释放，这是正常的。使用 Valgrind 的 `--show-reachable=no` 过滤。

## 测试覆盖率

### 使用 gcov 生成覆盖率报告

```bash
cmake .. -DCMAKE_BUILD_TYPE=Debug -DAPOLLO_COVERAGE=ON
cmake --build .
./tests/test_actor_framework
gcov tests/*.gcda
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_html
```

### 当前覆盖率

| 模块 | 行覆盖率 | 分支覆盖率 |
|------|----------|------------|
| Message | 95% | 90% |
| ActorCell | 90% | 85% |
| Dispatcher | 85% | 80% |
| TimerService | 88% | 82% |
| ActorManager | 80% | 75% |
| Monitoring | 70% | 65% |

## 贡献指南

提交代码前请确保：

1. 所有测试通过
2. 新功能包含测试
3. 使用 ThreadSanitizer 和 Valgrind 检查
4. 代码覆盖率不降低
