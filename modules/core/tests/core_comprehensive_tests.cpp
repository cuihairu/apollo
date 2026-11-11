/**
 * @file core_comprehensive_tests.cpp
 * @brief Comprehensive test suite for apollo::core module
 *
 * Coverage targets:
 * - ConfigRegistry: 95%+
 * - LogManager: 95%+
 * - ApplicationContext (DI): 90%+
 * - ApplicationLifecycle: 100% (simple enum)
 */

#include "apollo/core/application_lifecycle.hpp"
#include "apollo/core/config/config_registry.hpp"
#include "apollo/core/di/application_context.hpp"
#include "apollo/core/log/file_appender.hpp"
#include "apollo/core/log/log_manager.hpp"
#include "apollo/core/log/log_record.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>

// ============================================================================
// Test Framework
// ============================================================================

#define TEST(name) bool test_##name()
#define ASSERT_TRUE(expr) do { if (!(expr)) { std::cerr << "FAILED: " #expr << " at " << __FILE__ << ":" << __LINE__ << std::endl; return false; } } while(0)
#define ASSERT_FALSE(expr) ASSERT_TRUE(!(expr))
#define ASSERT_EQ(a, b) ASSERT_TRUE((a) == (b))
#define ASSERT_NE(a, b) ASSERT_TRUE((a) != (b))
#define ASSERT_LT(a, b) ASSERT_TRUE((a) < (b))
#define ASSERT_GT(a, b) ASSERT_TRUE((a) > (b))
#define ASSERT_GE(a, b) ASSERT_TRUE((a) >= (b))
#define ASSERT_LE(a, b) ASSERT_TRUE((a) <= (b))
#define ASSERT_STREQ(a, b) ASSERT_TRUE(std::string(a) == std::string(b))
#define ASSERT_STRNE(a, b) ASSERT_TRUE(std::string(a) != std::string(b))
#define ASSERT_NULL(ptr) ASSERT_TRUE((ptr) == nullptr)
#define ASSERT_NOT_NULL(ptr) ASSERT_TRUE((ptr) != nullptr)

// ============================================================================
// ApplicationLifecycle Tests
// ============================================================================

TEST(lifecycle_enum_values) {
    using apollo::core::ApplicationPhase;

    ASSERT_EQ(static_cast<int>(ApplicationPhase::Boot), 0);
    ASSERT_EQ(static_cast<int>(ApplicationPhase::ConfigLoaded), 1);
    ASSERT_EQ(static_cast<int>(ApplicationPhase::Initialized), 2);
    ASSERT_EQ(static_cast<int>(ApplicationPhase::Ready), 3);
    ASSERT_EQ(static_cast<int>(ApplicationPhase::Stopping), 4);
    ASSERT_EQ(static_cast<int>(ApplicationPhase::Stopped), 5);

    return true;
}

// ============================================================================
// ConfigRegistry Tests
// ============================================================================

TEST(config_registry_set_string) {
    auto& cfg = apollo::core::config::global_config();

    cfg.set("server.name", "apollo");
    cfg.set("server.host", "localhost");
    cfg.set("empty", "");

    ASSERT_EQ(cfg.get_string("server.name"), "apollo");
    ASSERT_EQ(cfg.get_string("server.host"), "localhost");
    ASSERT_EQ(cfg.get_string("empty"), "");
    ASSERT_EQ(cfg.get_string("missing", "default"), "default");

    return true;
}

TEST(config_registry_set_int64) {
    auto& cfg = apollo::core::config::global_config();

    cfg.set("port", "8080");
    cfg.set("timeout", "30");

    ASSERT_EQ(cfg.get_int64("port"), 8080);
    ASSERT_EQ(cfg.get_int64("timeout"), 30);
    ASSERT_EQ(cfg.get_int64("missing", 100), 100);

    // Invalid number returns default
    ASSERT_EQ(cfg.get_int64("port", 100), 8080);  // Valid
    cfg.set("invalid", "not_a_number");
    ASSERT_EQ(cfg.get_int64("invalid", 999), 999);

    return true;
}

TEST(config_registry_set_bool) {
    auto& cfg = apollo::core::config::global_config();

    cfg.set("enabled", "true");
    cfg.set("disabled", "false");
    cfg.set("one", "1");
    cfg.set("zero", "0");
    cfg.set("yes", "yes");
    cfg.set("no", "no");

    ASSERT_TRUE(cfg.get_bool("enabled"));
    ASSERT_FALSE(cfg.get_bool("disabled"));
    ASSERT_TRUE(cfg.get_bool("one"));
    ASSERT_FALSE(cfg.get_bool("zero"));
    ASSERT_TRUE(cfg.get_bool("yes"));
    ASSERT_FALSE(cfg.get_bool("no"));

    ASSERT_TRUE(cfg.get_bool("missing", true));   // Default true
    ASSERT_FALSE(cfg.get_bool("missing", false));  // Default false

    // Invalid bool value returns default
    cfg.set("invalid", "maybe");
    ASSERT_TRUE(cfg.get_bool("invalid", true));
    ASSERT_FALSE(cfg.get_bool("invalid", false));

    return true;
}

TEST(config_registry_has) {
    auto& cfg = apollo::core::config::global_config();

    ASSERT_FALSE(cfg.has("test"));
    cfg.set("test", "value");
    ASSERT_TRUE(cfg.has("test"));

    return true;
}

TEST(config_registry_overwrite) {
    auto& cfg = apollo::core::config::global_config();

    cfg.set("key", "value1");
    ASSERT_EQ(cfg.get_string("key"), "value1");

    cfg.set("key", "value2");
    ASSERT_EQ(cfg.get_string("key"), "value2");

    return true;
}

TEST(config_registry_nullptr) {
    auto& cfg = apollo::core::config::global_config();

    // Test nullptr value
    cfg.set("null_key", (const char*)nullptr);
    ASSERT_EQ(cfg.get_string("null_key"), "");

    return true;
}

TEST(config_registry_concurrent_access) {
    auto& cfg = apollo::core::config::global_config();

    // Concurrent writes
    std::thread t1([&]() {
        for (int i = 0; i < 100; ++i) {
            cfg.set("key1", std::to_string(i));
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 100; ++i) {
            cfg.set("key2", std::to_string(i));
        }
    });

    t1.join();
    t2.join();

    // Should have consistent values
    ASSERT_TRUE(cfg.has("key1"));
    ASSERT_TRUE(cfg.has("key2"));

    return true;
}

// ============================================================================
// LogManager Tests
// ============================================================================

TEST(log_manager_write) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    log.write(apollo::core::log::LogLevel::Info, "test", "message");

    auto entries = log.snapshot();
    ASSERT_EQ(entries.size(), 1);
    ASSERT_EQ(entries[0].category, "test");
    ASSERT_EQ(entries[0].message, "message");

    return true;
}

TEST(log_manager_levels) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    log.write(apollo::core::log::LogLevel::Debug, "cat", "debug");
    log.write(apollo::core::log::LogLevel::Info, "cat", "info");
    log.write(apollo::core::log::LogLevel::Warning, "cat", "warn");
    log.write(apollo::core::log::LogLevel::Error, "cat", "error");

    auto entries = log.snapshot();
    ASSERT_EQ(entries.size(), 4);

    ASSERT_EQ(entries[0].level, apollo::core::log::LogLevel::Debug);
    ASSERT_EQ(entries[1].level, apollo::core::log::LogLevel::Info);
    ASSERT_EQ(entries[2].level, apollo::core::log::LogLevel::Warning);
    ASSERT_EQ(entries[3].level, apollo::core::log::LogLevel::Error);

    return true;
}

TEST(log_manager_clear) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    log.write(apollo::core::log::LogLevel::Info, "test", "message");
    ASSERT_EQ(log.snapshot().size(), 1);

    log.clear();
    ASSERT_EQ(log.snapshot().size(), 0);

    return true;
}

TEST(log_manager_console_enabled) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    log.set_console_enabled(false);
    log.write(apollo::core::log::LogLevel::Info, "test", "message");
    // Should not crash, but we can't test console output easily

    log.set_console_enabled(true);
    // Console output should appear

    return true;
}

TEST(log_manager_concurrent) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    // Concurrent writes from multiple threads
    std::thread t1([&]() {
        for (int i = 0; i < 50; ++i) {
            log.write(apollo::core::log::LogLevel::Info, "t1", "msg" + std::to_string(i));
        }
    });

    std::thread t2([&]() {
        for (int i = 0; i < 50; ++i) {
            log.write(apollo::core::log::LogLevel::Info, "t2", "msg" + std::to_string(i));
        }
    });

    t1.join();
    t2.join();

    auto entries = log.snapshot();
    ASSERT_EQ(entries.size(), 100);

    return true;
}

TEST(log_manager_to_string) {
    using apollo::core::log::to_string;

    ASSERT_EQ(to_string(apollo::core::log::LogLevel::Debug), "DEBUG");
    ASSERT_EQ(to_string(apollo::core::log::LogLevel::Info), "INFO");
    ASSERT_EQ(to_string(apollo::core::log::LogLevel::Warning), "WARN");
    ASSERT_EQ(to_string(apollo::core::log::LogLevel::Error), "ERROR");

    return true;
}

// ============================================================================
// FileAppender Tests
// ============================================================================

namespace file_appender_test {

std::string read_file(const std::string& path) {
    std::ifstream file(path);
    return std::string(std::istreambuf_iterator<char>(file),
                       std::istreambuf_iterator<char>());
}

using apollo::core::log::FileAppender;
using apollo::core::log::FileAppenderConfig;
using apollo::core::log::LogRecord;
using apollo::core::log::LogRotationMode;

} // namespace file_appender_test

TEST(file_appender_write_and_resume) {
    using namespace file_appender_test;
    namespace fs = std::filesystem;

    const std::string dir = "test_logs_core_resume";
    std::error_code ec;
    fs::remove_all(dir, ec);

    FileAppenderConfig config;
    config.directory = dir;
    config.baseName = "resume";
    config.extension = "log";
    config.rotationMode = LogRotationMode::ByBoth;

    std::string first_path;
    {
        FileAppender appender(config);
        appender.append(LogRecord(apollo::core::log::LogLevel::Info, "first"));
        first_path = appender.getCurrentFilePath();
    }

    // 同日重启续接：新实例扫描既有轮转文件取 max 序号 +1，不覆写旧文件
    FileAppender resumed(config);
    const std::string resumed_path = resumed.getCurrentFilePath();
    ASSERT_TRUE(resumed_path != first_path);
    ASSERT_TRUE(resumed_path.find("_1.") != std::string::npos);

    resumed.append(LogRecord(apollo::core::log::LogLevel::Info, "second"));
    resumed.flush();

    ASSERT_TRUE(fs::exists(first_path));
    ASSERT_TRUE(fs::exists(resumed_path));
    ASSERT_TRUE(read_file(resumed_path).find("second") != std::string::npos);

    fs::remove_all(dir, ec);
    return true;
}

TEST(file_appender_rotation_cleanup) {
    using namespace file_appender_test;
    namespace fs = std::filesystem;

    const std::string dir = "test_logs_core_cleanup";
    std::error_code ec;
    fs::remove_all(dir, ec);

    FileAppenderConfig config;
    config.directory = dir;
    config.baseName = "clean";
    config.extension = "log";
    config.rotationMode = LogRotationMode::BySize;
    config.maxFileSize = 64;   // 小文件强制轮转
    config.maxFiles = 2;       // 只保留 2 个

    {
        FileAppender appender(config);
        for (int i = 0; i < 6; ++i) {
            appender.append(LogRecord(apollo::core::log::LogLevel::Info,
                                      "cleanup rotation record 0123456789"));
        }
        appender.rotate();  // 手动轮转同样触发清理
        appender.flush();
    }

    size_t kept = 0;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() &&
            entry.path().filename().string().find("clean_") == 0) {
            ++kept;
        }
    }
    ASSERT_TRUE(kept <= 2);

    fs::remove_all(dir, ec);
    return true;
}

TEST(file_appender_level_filter) {
    using namespace file_appender_test;
    namespace fs = std::filesystem;

    const std::string dir = "test_logs_core_filter";
    std::error_code ec;
    fs::remove_all(dir, ec);

    FileAppenderConfig config;
    config.directory = dir;
    config.baseName = "filter";
    config.extension = "log";
    config.rotationMode = LogRotationMode::None;

    FileAppender appender(config);
    // isEnabled 为位掩码口径（IAppender::hasLevel）：掩码含 Error 才放行 Error，
    // 与阈值语义（Warning 及以上）不同
    appender.setLevel(apollo::core::log::LogLevel::Warning |
                      apollo::core::log::LogLevel::Error);

    appender.append(LogRecord(apollo::core::log::LogLevel::Info, "filtered_out"));
    appender.append(LogRecord(apollo::core::log::LogLevel::Error, "error_passed"));
    appender.flush();

    const std::string content = read_file(appender.getCurrentFilePath());
    ASSERT_TRUE(content.find("filtered_out") == std::string::npos);
    ASSERT_TRUE(content.find("error_passed") != std::string::npos);

    fs::remove_all(dir, ec);
    return true;
}

TEST(file_appender_structured_output) {
    using namespace file_appender_test;
    namespace fs = std::filesystem;

    const std::string dir = "test_logs_core_structured";
    std::error_code ec;
    fs::remove_all(dir, ec);

    FileAppenderConfig config;
    config.directory = dir;
    config.baseName = "structured";
    config.extension = "log";
    config.rotationMode = LogRotationMode::None;
    config.structuredOutput = true;   // §5.1 logfmt 行契约

    FileAppender appender(config);
    appender.append(LogRecord(apollo::core::log::LogLevel::Info, "hello"));
    appender.flush();

    const std::string content = read_file(appender.getCurrentFilePath());
    ASSERT_TRUE(content.rfind("ts=", 0) == 0);
    ASSERT_TRUE(content.find("level=INFO") != std::string::npos);
    ASSERT_TRUE(content.find("msg=hello") != std::string::npos);
    // 结构化模式不落人读格式
    ASSERT_TRUE(content.find("[INFO]") == std::string::npos);

    fs::remove_all(dir, ec);
    return true;
}

TEST(file_appender_set_config_reopen) {
    using namespace file_appender_test;
    namespace fs = std::filesystem;

    const std::string dir_a = "test_logs_core_reopen_a";
    const std::string dir_b = "test_logs_core_reopen_b";
    std::error_code ec;
    fs::remove_all(dir_a, ec);
    fs::remove_all(dir_b, ec);

    FileAppenderConfig config_a;
    config_a.directory = dir_a;
    config_a.baseName = "reopen";
    config_a.extension = "log";
    config_a.rotationMode = LogRotationMode::None;

    FileAppender appender(config_a);
    appender.append(LogRecord(apollo::core::log::LogLevel::Info, "before_switch"));
    appender.flush();

    FileAppenderConfig config_b = config_a;
    config_b.directory = dir_b;
    appender.setConfig(config_b);   // 换目录重开，续接口径与构造一致
    appender.append(LogRecord(apollo::core::log::LogLevel::Info, "after_switch"));
    appender.flush();

    const std::string path_b = appender.getCurrentFilePath();
    ASSERT_TRUE(path_b.find("test_logs_core_reopen_b") != std::string::npos);
    ASSERT_TRUE(read_file(path_b).find("after_switch") != std::string::npos);

    // 旧目录文件原样保留，未被覆写
    bool old_kept = false;
    for (const auto& entry : fs::directory_iterator(dir_a)) {
        if (read_file(entry.path().string()).find("before_switch") !=
            std::string::npos) {
            old_kept = true;
        }
    }
    ASSERT_TRUE(old_kept);

    fs::remove_all(dir_a, ec);
    fs::remove_all(dir_b, ec);
    return true;
}

// ============================================================================
// ApplicationContext (DI) Tests
// ============================================================================

// Test services
struct IService {
    virtual ~IService() = default;
    virtual std::string name() const = 0;
};

struct DatabaseService {
    DatabaseService() : id_(0) {}
    explicit DatabaseService(int id) : id_(id) {}

    int get_id() const { return id_; }

private:
    int id_;
};

struct CacheService {
    explicit CacheService(const DatabaseService& db)
        : db_id_(db.get_id()) {}

    int get_db_id() const { return db_id_; }

private:
    int db_id_;
};

struct UserService : IService {
    UserService(CacheService& cache, DatabaseService& db)
        : cache_(cache), db_(db) {}

    std::string name() const override { return "UserService"; }

    int get_cache_db_id() const { return cache_.get_db_id(); }
    int get_db_id() const { return db_.get_id(); }

private:
    CacheService& cache_;
    DatabaseService& db_;
};

TEST(di_singleton_resolution) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>()
        .name("db");

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    auto& db = context.get<DatabaseService>();
    ASSERT_EQ(db.get_id(), 0);  // Default id

    // Same instance on second access
    auto& db2 = context.get<DatabaseService>();
    ASSERT_EQ(&db, &db2);

    context.shutdown();
    return true;
}

TEST(di_constructor_injection) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>();
    builder.add_singleton<CacheService, DatabaseService>();
    builder.add_singleton<UserService, CacheService, DatabaseService>()
        .as<IService>();

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Dependencies should be injected
    auto& user = context.get<UserService>();
    ASSERT_EQ(user.name(), "UserService");

    auto* service = context.try_get<IService>();
    ASSERT_NOT_NULL(service);
    ASSERT_EQ(service->name(), "UserService");

    context.shutdown();
    return true;
}

TEST(di_named_bean) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>()
        .name("primary_db");

    builder.add_singleton<DatabaseService>()
        .name("secondary_db");

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    auto& db1 = context.get_named<DatabaseService>("primary_db");
    auto& db2 = context.get_named<DatabaseService>("secondary_db");

    ASSERT_NE(&db1, &db2);  // Different instances

    context.shutdown();
    return true;
}

TEST(di_prototype_scope) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_prototype<DatabaseService>();

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Each create() returns a new instance
    auto bean1 = context.create<DatabaseService>();
    auto bean2 = context.create<DatabaseService>();

    ASSERT_NOT_NULL(bean1.get());
    ASSERT_NOT_NULL(bean2.get());
    ASSERT_NE(bean1.get(), bean2.get());  // Different pointers

    context.shutdown();
    return true;
}

TEST(di_missing_bean) {
    apollo::core::di::ApplicationContextBuilder builder;

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Missing bean should return nullptr
    auto* missing = context.try_get<DatabaseService>();
    ASSERT_NULL(missing);

    context.shutdown();
    return true;
}

TEST(di_tags) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>()
        .name("main_db")
        .tag("production")
        .tag("master");

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Bean should be accessible
    auto& db = context.get_named<DatabaseService>("main_db");
    ASSERT_GT(db.get_id(), -1);

    context.shutdown();
    return true;
}

TEST(di_eager_lazy) {
    apollo::core::di::ApplicationContextBuilder builder;

    // DI 注入按类型解析 bean——计数器本身作为 bean，服务以 bean 引用注入
    struct CounterService {
        int count = 0;
    };

    struct EagerService {
        explicit EagerService(CounterService& counter) { ++counter.count; }
    };

    struct LazyService {
        explicit LazyService(CounterService& counter) { ++counter.count; }
    };

    builder.add_singleton<CounterService>();

    // Add eager (default for singleton)
    builder.add_singleton<EagerService, CounterService>()
        .eager(true);

    // Add lazy
    builder.add_singleton<LazyService, CounterService>()
        .eager(false);

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Eager service should be constructed during initialize
    ASSERT_EQ(context.get<CounterService>().count, 1);

    // Access lazy service to trigger construction
    context.get<LazyService>();
    ASSERT_EQ(context.get<CounterService>().count, 2);

    context.shutdown();
    return true;
}

TEST(di_multiple_interfaces) {
    apollo::core::di::ApplicationContextBuilder builder;

    struct IMulti1 { virtual ~IMulti1() = default; };
    struct IMulti2 { virtual ~IMulti2() = default; };

    struct MultiImpl : IMulti1, IMulti2 {
        int value = 42;
    };

    builder.add_singleton<MultiImpl>()
        .as<IMulti1>()
        .as<IMulti2>();

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    auto& multi1 = context.get<IMulti1>();
    auto& multi2 = context.get<IMulti2>();

    // Same underlying object
    auto& impl = static_cast<MultiImpl&>(multi1);
    ASSERT_EQ(impl.value, 42);

    context.shutdown();
    return true;
}

TEST(di_get_all) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>()
        .name("db1");

    builder.add_singleton<DatabaseService>()
        .name("db2");

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // get_all should return all DatabaseService instances
    auto dbs = context.get_all<DatabaseService>();
    ASSERT_EQ(dbs.size(), 2);

    context.shutdown();
    return true;
}

namespace di_test_flags {
// 文件作用域旗标：shutdown 连 flags bean 一起销毁，不能把旗标放 bean 里读
static bool lc_constructed = false;
static bool lc_destroyed = false;
}

struct LifeCycleService {
    LifeCycleService() { di_test_flags::lc_constructed = true; }
    ~LifeCycleService() { di_test_flags::lc_destroyed = true; }
};

TEST(di_lifecycle) {
    apollo::core::di::ApplicationContextBuilder builder;
    di_test_flags::lc_constructed = false;
    di_test_flags::lc_destroyed = false;

    builder.add_singleton<LifeCycleService>();

    {
        auto context = builder.build();
        ASSERT_TRUE(context.initialize());
        ASSERT_TRUE(di_test_flags::lc_constructed);
        ASSERT_FALSE(di_test_flags::lc_destroyed);

        context.shutdown();
        ASSERT_TRUE(di_test_flags::lc_destroyed);
    }

    return true;
}

TEST(di_context_move) {
    apollo::core::di::ApplicationContextBuilder builder;

    builder.add_singleton<DatabaseService>();

    auto context1 = builder.build();
    ASSERT_TRUE(context1.initialize());

    // Move construct
    apollo::core::di::ApplicationContext context2 = std::move(context1);

    // context2 should still work
    auto& db = context2.get<DatabaseService>();
    ASSERT_GT(db.get_id(), -1);

    context2.shutdown();
    return true;
}

// ============================================================================
// Integration Tests
// ============================================================================

TEST(config_integration) {
    // 局部实例：global_config 无 clear()，全链路用可弃置的局部 registry
    apollo::core::config::ConfigRegistry cfg;

    cfg.set("app.name", "Apollo");
    cfg.set("app.version", "1.0.0");
    cfg.set("app.debug", true);
    cfg.set("app.port", int64_t{8080});

    // Verify all values
    ASSERT_EQ(cfg.get_string("app.name"), "Apollo");
    ASSERT_EQ(cfg.get_string("app.version"), "1.0.0");
    ASSERT_TRUE(cfg.get_bool("app.debug"));
    ASSERT_EQ(cfg.get_int64("app.port"), 8080);

    // Test nested config access
    ASSERT_TRUE(cfg.has("app.name"));

    return true;
}

TEST(log_integration) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    // Set up logging
    log.set_console_enabled(false);

    // Write various levels
    log.write(apollo::core::log::LogLevel::Debug, "system", "debug message");
    log.write(apollo::core::log::LogLevel::Info, "system", "info message");
    log.write(apollo::core::log::LogLevel::Warning, "system", "warn message");
    log.write(apollo::core::log::LogLevel::Error, "system", "error message");

    auto entries = log.snapshot();
    ASSERT_EQ(entries.size(), 4);

    // Verify level ordering
    ASSERT_EQ(entries[0].level, apollo::core::log::LogLevel::Debug);
    ASSERT_EQ(entries[1].level, apollo::core::log::LogLevel::Info);
    ASSERT_EQ(entries[2].level, apollo::core::log::LogLevel::Warning);
    ASSERT_EQ(entries[3].level, apollo::core::log::LogLevel::Error);

    // Clear and verify
    log.clear();
    ASSERT_EQ(log.snapshot().size(), 0);

    return true;
}

TEST(di_with_config_integration) {
    // Set up config values
    auto& cfg = apollo::core::config::global_config();
    cfg.set("di.db.port", int64_t{5432});
    cfg.set("di.db.host", "localhost");
    cfg.set("di.cache.enabled", true);

    // Use config in DI——服务默认构造时从 global_config 读取
    apollo::core::di::ApplicationContextBuilder builder;

    struct ConfiguredService {
        ConfiguredService() {
            auto& cfg = apollo::core::config::global_config();
            port_ = static_cast<int>(cfg.get_int64("di.db.port"));
            host_ = cfg.get_string("di.db.host");
            cache_enabled_ = cfg.get_bool("di.cache.enabled");
        }

        int port_;
        std::string host_;
        bool cache_enabled_;
    };

    builder.add_singleton<ConfiguredService>();

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    auto& service = context.get<ConfiguredService>();
    ASSERT_EQ(service.port_, 5432);
    ASSERT_EQ(service.host_, "localhost");
    ASSERT_TRUE(service.cache_enabled_);

    context.shutdown();

    return true;
}

TEST(di_with_logging_integration) {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();

    // Service that logs during construction
    struct LoggingService {
        LoggingService() {
            auto& log = apollo::core::log::global_log_manager();
            log.write(apollo::core::log::LogLevel::Info, "Lifecycle", "LoggingService constructed");
        }
    };

    apollo::core::di::ApplicationContextBuilder builder;
    builder.add_singleton<LoggingService>();

    auto context = builder.build();
    ASSERT_TRUE(context.initialize());

    // Should have logged construction
    auto entries = log.snapshot();
    ASSERT_GT(entries.size(), 0);

    // Find the construction log
    bool found = false;
    for (const auto& entry : entries) {
        if (entry.category == "Lifecycle" && entry.message == "LoggingService constructed") {
            found = true;
            break;
        }
    }
    ASSERT_TRUE(found);

    context.shutdown();
    log.clear();

    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "=== Apollo Core Module Comprehensive Tests ===" << std::endl;
    std::cout << std::endl;

    int passed = 0;
    int failed = 0;

    #define RUN_TEST(name) \
        do { \
            std::cout << "Running test_" #name "... "; \
            if (test_##name()) { \
                std::cout << "PASSED" << std::endl; \
                ++passed; \
            } else { \
                std::cout << "FAILED" << std::endl; \
                ++failed; \
            } \
        } while(0)

    // ApplicationLifecycle Tests
    std::cout << "--- ApplicationLifecycle Tests ---" << std::endl;
    RUN_TEST(lifecycle_enum_values);

    // ConfigRegistry Tests
    std::cout << "\n--- ConfigRegistry Tests ---" << std::endl;
    RUN_TEST(config_registry_set_string);
    RUN_TEST(config_registry_set_int64);
    RUN_TEST(config_registry_set_bool);
    RUN_TEST(config_registry_has);
    RUN_TEST(config_registry_overwrite);
    RUN_TEST(config_registry_nullptr);
    RUN_TEST(config_registry_concurrent_access);

    // LogManager Tests
    std::cout << "\n--- LogManager Tests ---" << std::endl;
    RUN_TEST(log_manager_write);
    RUN_TEST(log_manager_levels);
    RUN_TEST(log_manager_clear);
    RUN_TEST(log_manager_console_enabled);
    RUN_TEST(log_manager_concurrent);
    RUN_TEST(log_manager_to_string);

    // FileAppender Tests
    std::cout << "\n--- FileAppender Tests ---" << std::endl;
    RUN_TEST(file_appender_write_and_resume);
    RUN_TEST(file_appender_rotation_cleanup);
    RUN_TEST(file_appender_level_filter);
    RUN_TEST(file_appender_structured_output);
    RUN_TEST(file_appender_set_config_reopen);

    // DI Tests
    std::cout << "\n--- ApplicationContext (DI) Tests ---" << std::endl;
    RUN_TEST(di_singleton_resolution);
    RUN_TEST(di_constructor_injection);
    RUN_TEST(di_named_bean);
    RUN_TEST(di_prototype_scope);
    RUN_TEST(di_missing_bean);
    RUN_TEST(di_tags);
    RUN_TEST(di_eager_lazy);
    RUN_TEST(di_multiple_interfaces);
    RUN_TEST(di_get_all);
    RUN_TEST(di_lifecycle);
    RUN_TEST(di_context_move);

    // Integration Tests
    std::cout << "\n--- Integration Tests ---" << std::endl;
    RUN_TEST(config_integration);
    RUN_TEST(log_integration);
    RUN_TEST(di_with_config_integration);
    RUN_TEST(di_with_logging_integration);

    #undef RUN_TEST

    std::cout << "\n=== Test Summary ===" << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << failed << std::endl;

    if (failed > 0) {
        std::cerr << "\nSome tests FAILED!" << std::endl;
        return 1;
    }

    std::cout << "\nAll tests PASSED!" << std::endl;
    return 0;
}
