#include "apollo/runtime/world_host.hpp"

#include <algorithm>

namespace apollo::runtime {

namespace {

constexpr std::uint32_t kDefaultTickRate = 20;

double compute_delta_seconds(
    const std::chrono::steady_clock::time_point& previous,
    const std::chrono::steady_clock::time_point& current,
    std::uint32_t tick_rate_hz) {
    if (previous.time_since_epoch().count() == 0) {
        return 1.0 / static_cast<double>(tick_rate_hz);
    }

    return std::chrono::duration<double>(current - previous).count();
}

} // namespace

WorldHost::WorldHost(std::uint32_t tick_rate_hz) {
    set_tick_rate(tick_rate_hz);
}

std::string_view WorldHost::service_name() const {
    return "WorldHost";
}

bool WorldHost::start() {
    tick_context_ = {};
    last_tick_time_ = {};

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

void WorldHost::tick() {
    if (!running_) {
        return;
    }

    const auto now = std::chrono::steady_clock::now();
    tick_context_.tick_index += 1;
    tick_context_.now = now;
    tick_context_.delta_seconds = compute_delta_seconds(last_tick_time_, now, tick_rate_hz_);
    last_tick_time_ = now;

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

} // namespace apollo::runtime
