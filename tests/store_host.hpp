#pragma once

// 独立 PlayerStore 宿主替身（L19 资料归属切换测试面）：store M1 面 +
// 锚点全量重报面同口径装配（对齐 apps/base-server/src/main.cpp），替换
// 原 WorldApp 宿主位。periodic 泵由测试侧 pump_until 循环驱动（同
// baseServer 主循环语义：启动即报 + interval 周期刷新，幂等 intake）。

#include "player_store/player_store.hpp"
#include "apollo/game/session/fleet_recovery.hpp"
#include "apollo/net/discovery/udp_transport.hpp"

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>

struct StoreHost {
    std::unique_ptr<player_store::PlayerStore> store;
    apollo::net::discovery::UdpBeaconTransport transport;
    std::unique_ptr<apollo::game::session::FleetReporter> reporter;
    std::uint32_t zone_id = 0;
    std::uint64_t report_interval_ms = 0;
    std::chrono::steady_clock::time_point last_report{};

    // report_port = 0 → 无报告面（纯 store 宿主）。开启时 component 号
    // 必给（FleetReporter 拒 0）。构造即启动即报（恢复相位全量重报）。
    StoreHost(player_store::PlayerStoreConfig store_config,
              std::uint16_t report_port, std::uint64_t component_id,
              std::uint32_t zone, std::uint64_t interval_ms)
        : zone_id(zone), report_interval_ms(interval_ms) {
        store = std::make_unique<player_store::PlayerStore>(std::move(store_config));
        store->start();
        if (report_port != 0) {
            if (component_id == 0)
                throw std::runtime_error("StoreHost report component id required");
            if (!transport.open())
                throw std::runtime_error("StoreHost report transport did not open");
            reporter = std::make_unique<apollo::game::session::FleetReporter>(
                component_id, zone_id,
                [this, report_port](const std::uint8_t* data, std::size_t len) {
                    return transport.send_bytes("127.0.0.1", report_port, data, len);
                });
            pump();
        }
    }

    // 周期全量重报泵：距上次报告 ≥ interval 才发（0 = 仅启动期一报）。
    void pump() {
        if (!reporter) return;
        const auto since = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::steady_clock::now() - last_report)
                               .count();
        if (since < static_cast<std::int64_t>(report_interval_ms)) return;
        (void)reporter->report_full(
            player_store::collect_mirror_entries(*store, zone_id));
        last_report = std::chrono::steady_clock::now();
    }

    // 报告面退场（死亡拓扑演练：停发后 intake restore-not-kick 不再复活）。
    void stop_reports() {
        reporter.reset();
        transport.close();
    }

    ~StoreHost() {
        stop_reports();
        if (store) store->stop();
    }
};
