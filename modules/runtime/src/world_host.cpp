#include "apollo/runtime/world_host.hpp"

#include <algorithm>
#include <cmath>
#include <iostream>

namespace apollo::runtime {

WorldHost::WorldHost(std::uint32_t tick_rate_hz, NowFn now_fn)
    : now_fn_(now_fn != nullptr ? now_fn : &std::chrono::steady_clock::now) {
    set_tick_rate(tick_rate_hz);
}

std::string_view WorldHost::service_name() const {
    return "WorldHost";
}

bool WorldHost::start() {
    tick_context_ = {};
    last_tick_time_ = {};
    accumulated_seconds_ = 0.0;
    dropped_ticks_ = 0;
    warned_ = false;
    tick_started_ = false;

    for (const auto& service : world_services_) {
        if (!service || !service->initialize()) {
            for (auto it = world_services_.rbegin(); it != world_services_.rend(); ++it) {
                if (*it) {
                    (*it)->shutdown();
                }
            }
            running_ = false;
            return false;
        }
    }

    running_ = true;
    return true;
}

void WorldHost::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    for (auto it = world_services_.rbegin(); it != world_services_.rend(); ++it) {
        if (*it) {
            (*it)->shutdown();
        }
    }
}

bool WorldHost::is_running() const {
    return running_;
}

// 固定步长驱动：以真实流逝时间累计，按步长推进；连跑上限 kCatchUpMaxSteps 步，
// 积压仍超则跳拍（丢弃整步、累计 dropped_ticks_、告警）。tick 号从 1 开始（0 为无效哨兵）。
void WorldHost::tick() {
    if (!running_ || now_fn_ == nullptr) {
        return;
    }

    const auto now = now_fn_();
    if (!tick_started_) {
        // 首帧：只记录起点，不推进（clock-and-time §3：首个完整 tick 计 1）
        tick_started_ = true;
        last_tick_time_ = now;
        accumulated_seconds_ = 0.0;
        return;
    }

    const double elapsed = std::chrono::duration<double>(now - last_tick_time_).count();
    last_tick_time_ = now;
    accumulated_seconds_ += elapsed;

    const double step_seconds = 1.0 / static_cast<double>(tick_rate_hz_);
    // 1ulp 容差：避免时钟抖动在步长边界上造成 1ulp 级偏差（double 表示 0.1s 步长不精确）
    constexpr double kEpsilon = 1e-9;

    std::size_t run_steps = 0;
    while (accumulated_seconds_ + kEpsilon >= step_seconds && run_steps < kCatchUpMaxSteps) {
        accumulated_seconds_ -= step_seconds;
        advance_one_tick(now);
        ++run_steps;
    }

    if (accumulated_seconds_ + kEpsilon >= step_seconds) {
        // 积压超过有界追赶上限：跳拍。丢弃多出的整步，保留不足一步的余量。
        // +kEpsilon：修正 double 整步除法在 1ulp 级的截断下偏（真商恰为整数时 trunc 可能少 1）
        const std::uint64_t dropped = static_cast<std::uint64_t>(
                                           (accumulated_seconds_ - step_seconds + kEpsilon) /
                                           step_seconds) +
                                      1;
        accumulated_seconds_ = std::fmod(accumulated_seconds_, step_seconds);
        // fmod 余数语义上必 < step，但 double 表示下可能紧贴 step 下沿（差 < 1ulp）：
        // 此时视为零余量，避免下一次无流逝调用被容差误判为满一步（「幽灵步」）。
        if (step_seconds - accumulated_seconds_ < kEpsilon) {
            accumulated_seconds_ = 0.0;
        }
        dropped_ticks_ += dropped;
        if (!warned_) {
            std::cerr << "[WorldHost] falling behind: dropping " << dropped
                      << " tick(s) (backlog exceeds catch-up limit " << kCatchUpMaxSteps
                      << " steps); subsequent drops counted in dropped_ticks()" << std::endl;
            warned_ = true;
        }
    }
}

void WorldHost::advance_one_tick(std::chrono::steady_clock::time_point now) {
    tick_context_.tick_index += 1;
    tick_context_.now = now;
    tick_context_.delta_seconds = 1.0 / static_cast<double>(tick_rate_hz_);

    for (const auto& service : world_services_) {
        if (service) {
            service->tick(tick_context_);
        }
    }
}

void WorldHost::set_tick_rate(std::uint32_t tick_rate_hz) {
    tick_rate_hz_ = std::max(tick_rate_hz, 1U);
}

std::uint32_t WorldHost::tick_rate() const {
    return tick_rate_hz_;
}

void WorldHost::add_world_service(std::shared_ptr<IWorldService> service) {
    if (service) {
        world_services_.push_back(std::move(service));
    }
}

const WorldTickContext& WorldHost::tick_context() const {
    return tick_context_;
}

std::size_t WorldHost::world_service_count() const {
    return world_services_.size();
}

std::uint64_t WorldHost::dropped_ticks() const {
    return dropped_ticks_;
}

} // namespace apollo::runtime