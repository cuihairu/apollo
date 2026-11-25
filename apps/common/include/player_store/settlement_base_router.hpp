#pragma once

// D12 逐 Base 结算选路（同区多 Base 部分成功切片）：cell 结算面按玩家解析
// 归属 Base 端点（917/918 目录解析面回带的 base_host/base_port——登录实拨
// 端点随落点成行），逐玩家拨 958 送分。目录面未配 / 解析失败 / 端点未声明
// = 单 Base 配置兜底（现状语义零回归）。
//
// 选路面（header-only 直编习语，同 store_dial.hpp）：
//   - player → 端点缓存：目录显式给出的端点即入缓存（= 兜底端点者亦然
//     ——目录显式归属是事实，不因恰为兜底而区别）；命中不重解析。失败
//     清条目——传输失败（无回包/回包畸形）与业务级失配（accepted=false，
//     锚点已迁走等）都清：下次重解析拿新端点（挂账重试自愈）。目录解析
//     失败/未声明的兜底落法不入缓存——目录面缺席不当作玩家归属。
//   - 端点 → StoreDial 懒装配：每端点一拨（同进程多 LinkManager 实例互
//     不相扰）；传输失败随条目一并拆除（下次重拨新链，不留死链）。兜底
//     端点复用宿主借用拨（config.default_dial = baseBusinessDial_，不重复
//     拨同端点）。
//   - 首拨前 waitUp（RpcClient::call_async 链路非 kUp 即败——懒装配后立即
//     调用会假败；waitUp 有界 3s 自带超时）。
//
// 线程纪律：send 只在内核线程结算段调用（同 businessSeq_ 独占域），不带锁。

#include "player_store/business_wire.hpp"
#include "player_store/store_dial.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>

#if APOLLO_NET_LINK_HAS_JSON

namespace player_store {

class SettlementBaseRouter {
public:
    struct Config {
        std::string route_json;
        std::string self_name = "world";
        // 单 Base 兜底端点（宿主 baseBusiness 配置同源；port 0 = 面关——
        // send 恒败挂账）。
        std::string default_host;
        std::uint16_t default_port = 0;
        // 兜底端点的借用拨（宿主 baseBusinessDial_；null = 兜底端点自装配）。
        StoreDial* default_dial = nullptr;
        // 目录解析面（917/918；port 0 = 不解析——恒走单 Base 兜底）。
        std::string directory_host;
        std::uint16_t directory_port = 0;
    };

    explicit SettlementBaseRouter(Config config) : config_(std::move(config)) {}

    ~SettlementBaseRouter() {
        for (auto& entry : dials_) {
            entry.second->shutdown();
        }
    }

    SettlementBaseRouter(const SettlementBaseRouter&) = delete;
    SettlementBaseRouter& operator=(const SettlementBaseRouter&) = delete;

    struct Endpoint {
        std::string host;
        std::uint16_t port = 0;

        bool operator==(const Endpoint&) const noexcept = default;
        bool operator!=(const Endpoint&) const noexcept = default;
        [[nodiscard]] bool declared() const noexcept {
            return port != 0 && !host.empty();
        }
    };

    // 结算落账：按玩家解析归属 Base（918 base_host/base_port；未声明 =
    // 单 Base 兜底），拨 958 送分。返回 Base 是否受理（accepted）——传输
    // 不可达/回包畸形/受理拒绝 = false（调用面挂账语义）。
    bool send(std::uint64_t player_id, const business::ScoreCreditRequest& request) {
        Endpoint ep;
        if (!cached_endpoint(player_id, ep)) {
            ep = resolve_via_directory(player_id);
            if (!ep.declared()) {
                ep = default_endpoint();  // 未声明：兜底（不固化——目录缺席不当作归属）
            } else {
                remember(player_id, ep);  // 目录显式端点：缓存（= 兜底者亦然）
            }
        }
        auto* dial = dial_for(ep);
        if (dial == nullptr) {
            forget(player_id);  // 兜底也面关：条目不粘毒
            return false;
        }
        const auto reply = dial->call(business::encodeFrame(request));
        business::ScoreCreditReply rep;
        apollo::protocol::SessionID sid = 0;
        if (!reply || !business::decodeFrame(*reply, rep, sid)) {
            // 传输域失效：清缓存 + 拆拨——补齐重试重解析（迁移后新端点）
            forget(player_id);
            drop_dial(ep);
            return false;
        }
        if (!rep.accepted) {
            // 业务级失配（锚点已迁走/离线/迁移中）：清缓存——挂账重试重
            // 解析（真实端点已随目录行更新），不粘旧端点
            forget(player_id);
            return false;
        }
        return true;
    }

    // 观测面（测试用）：目录解析次数 / 缓存命中次数。
    std::uint64_t resolve_count() const { return resolve_count_; }
    std::uint64_t cache_hit_count() const { return cache_hit_count_; }

private:
    Endpoint default_endpoint() const { return Endpoint{config_.default_host, config_.default_port}; }

    bool cached_endpoint(std::uint64_t player_id, Endpoint& out) {
        const auto it = endpoints_.find(player_id);
        if (it == endpoints_.end()) {
            return false;
        }
        out = it->second;
        ++cache_hit_count_;
        return true;
    }

    void remember(std::uint64_t player_id, const Endpoint& ep) {
        endpoints_[player_id] = ep;
    }

    void forget(std::uint64_t player_id) { endpoints_.erase(player_id); }

    // 目录解析（917 问 → 918 回带 base_host/base_port）。目录面未配/解析
    // 失败/端点未声明 = 返回未声明端点（调用面落兜底）。解析拨常驻（链路
    // 断开由泵自愈重连）；首拨前 waitUp（懒装配后立即调用会假败——call_
    // async 链路非 kUp 即败），首次连通后断开 = 快速失败（重试/补齐路径
    // 自愈，不逐次拖 3s 等待窗）。
    Endpoint resolve_via_directory(std::uint64_t player_id) {
        ++resolve_count_;
        if (config_.directory_port == 0) {
            return {};
        }
        if (directory_dial_ == nullptr) {
            directory_dial_ = std::make_unique<StoreDial>(
                config_.route_json, config_.self_name, "directory",
                config_.directory_host, config_.directory_port);
        }
        if (!directory_dial_->ready()) {
            return {};
        }
        if (!directory_up_seen_) {
            if (!directory_dial_->waitUp()) {
                return {};
            }
            directory_up_seen_ = true;
        } else if (!directory_dial_->peerUp()) {
            return {};
        }
        protocol::PlayerResolveRouteRequest req;
        req.playerId = player_id;
        const auto reply = directory_dial_->call(
            apollo::protocol::MessageCodec::encode(req));
        if (!reply) {
            return {};
        }
        const auto header = apollo::protocol::MessageCodec::parseHeader(*reply);
        if (static_cast<apollo::protocol::MessageType>(header.type) !=
            apollo::protocol::MessageType::PLAYER_RESOLVE_ROUTE_RESPONSE) {
            return {};
        }
        const std::vector<std::uint8_t> body(reply->begin() +
                                                 sizeof(apollo::protocol::MessageHeader),
                                             reply->end());
        apollo::protocol::PlayerResolveRouteResponse response;
        try {
            apollo::protocol::MessageCodec::decodeBody(body, response);
        } catch (const std::exception&) {
            return {};
        }
        if (!response.success) {
            return {};
        }
        return Endpoint{response.baseHost, static_cast<std::uint16_t>(response.basePort)};
    }

    StoreDial* dial_for(const Endpoint& ep) {
        if (!ep.declared()) {
            return nullptr;
        }
        if (ep == default_endpoint() && config_.default_dial != nullptr) {
            return config_.default_dial;
        }
        const std::string key = ep.host + ":" + std::to_string(ep.port);
        const auto it = dials_.find(key);
        if (it != dials_.end()) {
            return it->second.get();
        }
        auto dial = std::make_unique<StoreDial>(config_.route_json, config_.self_name,
                                                "base", ep.host, ep.port);
        if (!dial->ready() || !dial->waitUp()) {
            return nullptr;  // 装配/首拨不成：不留半拨（下次重装配）
        }
        dials_[key] = std::move(dial);
        return dials_[key].get();
    }

    void drop_dial(const Endpoint& ep) {
        const std::string key = ep.host + ":" + std::to_string(ep.port);
        const auto it = dials_.find(key);
        if (it == dials_.end()) {
            return;  // 借用拨归宿主管，不拆
        }
        it->second->shutdown();
        dials_.erase(it);
    }

    Config config_;
    std::unordered_map<std::uint64_t, Endpoint> endpoints_;
    std::unordered_map<std::string, std::unique_ptr<StoreDial>> dials_;
    std::unique_ptr<StoreDial> directory_dial_;
    bool directory_up_seen_ = false;
    std::uint64_t resolve_count_ = 0;
    std::uint64_t cache_hit_count_ = 0;
};

} // namespace player_store

#endif // APOLLO_NET_LINK_HAS_JSON
