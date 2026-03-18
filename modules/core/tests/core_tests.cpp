#include "apollo/core/application_lifecycle.hpp"
#include "apollo/core/config/config_registry.hpp"
#include "apollo/core/di/application_context.hpp"
#include "apollo/core/log/log_manager.hpp"

#include <iostream>
#include <string>

namespace {

struct IService {
    virtual ~IService() = default;
    virtual std::string name() const = 0;
};

struct ClockService : apollo::core::IApplicationLifecycle {
    std::string value = "clock";
};

struct DemoService final : IService {
    explicit DemoService(ClockService& clock) : clock_(clock) {}

    std::string name() const override {
        return "demo:" + clock_.value;
    }

    ClockService& clock_;
};

bool test_di_singleton_resolution() {
    apollo::core::di::ApplicationContextBuilder builder;
    builder.add_singleton<ClockService>().name("clock");
    builder.add_singleton<DemoService, ClockService>().as<IService>().name("demo");

    auto context = builder.build();
    if (!context.initialize()) {
        return false;
    }

    auto* service = context.try_get<IService>();
    if (service == nullptr || service->name() != "demo:clock") {
        return false;
    }

    auto* named = context.try_get_named<IService>("demo");
    return named != nullptr && named == service;
}

bool test_lifecycle_types() {
    using apollo::core::ApplicationPhase;
    return static_cast<int>(ApplicationPhase::Boot) == 0 &&
           static_cast<int>(ApplicationPhase::Stopped) == 5;
}

bool test_config_registry() {
    auto& cfg = apollo::core::config::global_config();
    cfg.set("server.name", "apollo");
    cfg.set("server.port", static_cast<int64_t>(7700));
    cfg.set("feature.enabled", true);

    return cfg.get_string("server.name") == "apollo" &&
           cfg.get_int64("server.port") == 7700 &&
           cfg.get_bool("feature.enabled") == true &&
           cfg.get_string("missing", "fallback") == "fallback";
}

bool test_log_manager() {
    auto& log = apollo::core::log::global_log_manager();
    log.clear();
    log.set_console_enabled(false);
    log.write(apollo::core::log::LogLevel::Info, "core_tests", "hello");
    auto entries = log.snapshot();
    return entries.size() == 1 &&
           entries[0].category == "core_tests" &&
           entries[0].message == "hello";
}

} // namespace

int main() {
    const bool ok =
        test_di_singleton_resolution() &&
        test_lifecycle_types() &&
        test_config_registry() &&
        test_log_manager();

    if (!ok) {
        std::cerr << "apollo_core_tests failed" << std::endl;
        return 1;
    }

    std::cout << "apollo_core_tests passed" << std::endl;
    return 0;
}
