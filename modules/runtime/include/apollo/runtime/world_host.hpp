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

// 世界宿主：固定步长全序推进（clock-and-time §3 口径）。
//
// 设计口径（docs/design/clock-and-time.md §3 与 term-contract §1.5）：
// - 固定步长 100ms（10Hz）默认，与 BW DEFAULT_GAME_UPDATE_HERTZ=10 / KBE gameUpdateHertz=10 一致；
// - 追赶有界：单次 tick() 调用最多连跑 kCatchUpMaxSteps 步补齐积压，超出跳拍 + 告警 + 计 dropped_ticks；
// - tick 号进程内单调、重启归零、0 保留为「未启动/无效」哨兵，首个完整 tick 计 1。
//
// 时间源可注入（now_fn 构造参数），便于确定性单测；默认 steady_clock。
class WorldHost final : public IHostedService {
public:
    using NowFn = std::chrono::steady_clock::time_point (*)();

    // 契约/文档口径：10Hz（clock-and-time §3，BW/KBE 同型）；配置化就绪点见 P3-1
    static constexpr std::uint32_t kDefaultTickRate = 10;

    explicit WorldHost(std::uint32_t tick_rate_hz = kDefaultTickRate,
                       NowFn now_fn = &std::chrono::steady_clock::now);

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

    // 跳拍计数（不满足有界追赶而丢弃的完整 tick 步数）
    std::uint64_t dropped_ticks() const;

private:
    // 有界追赶上限（10Hz 下 5 步 = 500ms 积压预算）；配置化就绪点：P3-1 配置统一批次接入 core::config
    static constexpr std::size_t kCatchUpMaxSteps = 5;

    void advance_one_tick(std::chrono::steady_clock::time_point now);

    std::vector<std::shared_ptr<IWorldService>> world_services_;
    WorldTickContext tick_context_{};
    std::chrono::steady_clock::time_point last_tick_time_{};
    std::uint32_t tick_rate_hz_ = 10;
    double accumulated_seconds_ = 0.0;
    std::uint64_t dropped_ticks_ = 0;
    bool warned_ = false;
    bool running_ = false;
    // 首帧标记：显式布尔而非 time_point==0 判定（注入时钟下 epoch-0 时刻是合法输入，
    // 且语义上「未开始」必须与「恰好停在 epoch」可区分；start() 归零）
    bool tick_started_ = false;
    NowFn now_fn_ = nullptr;
};

} // namespace apollo::runtime
