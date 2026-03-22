#pragma once

#include "apollo/runtime/application_host.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <string_view>
#include <vector>

namespace apollo::runtime {

struct WorldTickContext {
    std::uint64_t tick_index = 0;
    double delta_seconds = 0.0;
    std::chrono::steady_clock::time_point now{};
};

class IWorldService {
public:
    virtual ~IWorldService() = default;

    virtual std::string_view service_name() const = 0;
    virtual bool initialize() = 0;
    virtual void tick(const WorldTickContext& context) = 0;
    virtual void shutdown() = 0;
};

class WorldHost final : public IHostedService {
public:
    explicit WorldHost(std::uint32_t tick_rate_hz = 20);

    std::string_view service_name() const override;
    bool start() override;
    void stop() override;
    bool is_running() const override;
    void tick() override;

    void set_tick_rate(std::uint32_t tick_rate_hz);
    std::uint32_t tick_rate() const;

    void add_world_service(std::shared_ptr<IWorldService> service);

    const WorldTickContext& tick_context() const;
    std::size_t world_service_count() const;

private:
    std::vector<std::shared_ptr<IWorldService>> world_services_;
    WorldTickContext tick_context_{};
    std::chrono::steady_clock::time_point last_tick_time_{};
    std::uint32_t tick_rate_hz_ = 20;
    bool running_ = false;
};

} // namespace apollo::runtime
