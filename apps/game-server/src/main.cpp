#include "apollo/runtime/crash_capture.hpp"
#include "apollo/core/module_manifest.hpp"
#include "apollo/base/id_pool.hpp"
#include "apollo/base/thread_pool.hpp"
#include "apollo/base/time.hpp"
#include "apollo/core/application_lifecycle.hpp"
#include "apollo/core/config/config_registry.hpp"
#include "apollo/core/di/application_context.hpp"
#include "apollo/core/log/log_manager.hpp"
#include "apollo/data/core/data_source.hpp"
#include "apollo/data/orm/memory_connection.hpp"
#include "apollo/data/orm/sql_template.hpp"
#include "apollo/runtime/application_host.hpp"
#include "apollo/runtime/runtime_manifest.hpp"
#include "apollo/runtime/service_host.hpp"

#include <iostream>

namespace {

// 日志接线（ADR-013 L1+L2）：console 人读 + 本地结构化文件真相源并行；
// push 出口留接口不实现（collector 面 M1 后，logging.md §5）
apollo::core::log::LoggerPtr initLogging() {
    auto& logs = apollo::core::log::global_log_manager();
    apollo::core::log::LogManagerConfig config;
    config.processIdentity = "game";
    config.fileEnabled = true;
    config.fileConfig.directory = "log";
    config.fileConfig.baseName = "game";
    config.fileConfig.structuredOutput = true;  // §5.1 结构化行（六键固定序）
    logs.initialize(config);
    return logs.createLogger("game");
}

struct GameClockService {
    std::string name = "clock";
};

struct LoginPipeline {
    explicit LoginPipeline(GameClockService& clock) : clock(clock) {}
    GameClockService& clock;
};

struct GameServerService final : apollo::runtime::IHostedService {
    explicit GameServerService(apollo::core::di::ApplicationContext& context)
        : context_(context) {}

    std::string_view service_name() const override {
        return "game_server";
    }

    bool start() override {
        running_ = true;
        pipeline_ = &context_.get<LoginPipeline>();
        return pipeline_ != nullptr;
    }

    void stop() override {
        running_ = false;
    }

    bool is_running() const override {
        return running_;
    }

    void tick() override {
        ++ticks_;
    }

    int ticks() const { return ticks_; }
    const LoginPipeline* pipeline() const { return pipeline_; }

private:
    apollo::core::di::ApplicationContext& context_;
    LoginPipeline* pipeline_ = nullptr;
    bool running_ = false;
    int ticks_ = 0;
};

} // namespace

int main(int argc, char* argv[]) {
    // 先于 crash capture：write() 在未初始化时会惰性按默认配置建管理器（仅 console），
    // 顺序颠倒会使本进程的文件面配置被默认初始化顶掉（initialize 幂等早退）
    const auto logger = initLogging();
    apollo::runtime::init_crash_capture(argc, argv, "game-server");
    auto& config = apollo::core::config::global_config();
    config.set("server.name", "apollo_game_server");
    config.set("server.max_players", static_cast<int64_t>(2000));
    auto& logs = apollo::core::log::global_log_manager();
    logs.clear();
    logs.set_console_enabled(false);
    logs.write(apollo::core::log::LogLevel::Info, "game-server", "bootstrap begin");
    auto connection = std::make_shared<apollo::data::orm::MemoryConnection>();
    connection->seed_query(
        "SELECT name, region FROM shard WHERE id = 1",
        {{{"name", "apollo-1"}, {"region", "cn-east"}}});
    auto data_source = std::make_shared<apollo::data::core::SimpleDataSource>(
        [connection]() { return connection; });
    apollo::data::orm::SqlTemplate sql(data_source);
    auto shard_name = sql.query_for_one<std::string>(
        "SELECT name, region FROM shard WHERE id = 1",
        [](const apollo::data::core::QueryRow& row) {
            auto it = row.find("name");
            return it != row.end() ? it->second : std::string{};
        });

    apollo::base::IdPool player_ids(1000, 1003);
    const auto allocated_id = player_ids.allocate();
    const auto boot_time = apollo::base::Time::format_now();
    apollo::base::ThreadPool pool(2);
    auto future = pool.submit([]() { return 7 * 6; });
    const auto worker_result = future.get();
    const auto phase = apollo::core::ApplicationPhase::Boot;
    apollo::core::di::ApplicationContextBuilder builder;
    builder.add_singleton<GameClockService>().name("game_clock");
    builder.add_singleton<LoginPipeline, GameClockService>().name("login_pipeline");
    auto context = builder.build();
    if (!context.initialize()) {
        return 1;
    }
    auto service = std::make_shared<GameServerService>(context);
    bool shutdown_hook_called = false;
    apollo::runtime::ServiceHost host;
    host.add_service(service);
    host.add_shutdown_hook([&](apollo::runtime::StopReason) {
        shutdown_hook_called = true;
    });
    if (!host.start()) {
        return 1;
    }
    host.run_once();
    host.request_stop(apollo::runtime::StopReason::Completed);
    host.run_once();

    const auto& core = apollo::core::core_manifest();
    const auto& runtime = apollo::runtime::runtime_manifest();
    const auto entries = logs.snapshot();

    std::cout << "Apollo game-server modular bootstrap" << '\n';
    std::cout << "server_name: " << config.get_string("server.name") << '\n';
    std::cout << "server_max_players: " << config.get_int64("server.max_players") << '\n';
    std::cout << "shard_name: " << (shard_name ? *shard_name : std::string{"unknown"}) << '\n';
    std::cout << "boot_time: " << boot_time << '\n';
    std::cout << "allocated_player_id: " << allocated_id << '\n';
    std::cout << "worker_result: " << worker_result << '\n';
    std::cout << "phase: " << static_cast<int>(phase) << '\n';
    std::cout << "host_phase: " << static_cast<int>(host.phase()) << '\n';
    std::cout << "service_ticks: " << service->ticks() << '\n';
    std::cout << "shutdown_hook_called: " << (shutdown_hook_called ? "true" : "false") << '\n';
    std::cout << "log_entries: " << entries.size() << '\n';
    std::cout << "login_pipeline_clock: " << service->pipeline()->clock.name << '\n';
    std::cout << "core: " << core.name << " (" << core.description << ")" << '\n';
    std::cout << "runtime: " << runtime.name << " (" << runtime.description << ")" << '\n';
    return 0;
}
