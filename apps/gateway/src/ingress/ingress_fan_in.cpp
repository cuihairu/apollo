#include "gateway/ingress/ingress_fan_in.hpp"

#include <algorithm>
#include <stdexcept>
#include <utility>

namespace gateway {

namespace {

class IngressFanIn final : public ClientIngressServer {
public:
    IngressFanIn(std::vector<IngressFanInBinding> bindings, std::uint64_t stride)
        : bindings_(std::move(bindings)), stride_(stride) {
        if (bindings_.empty()) {
            throw std::invalid_argument("ingress fan-in needs at least one child");
        }
        // 号段互斥是 send/close 路由正确性的前提——重叠即装配拒绝。
        for (std::size_t i = 0; i < bindings_.size(); ++i) {
            const std::uint64_t lo = bindings_[i].connectionIdBase + 1;
            const std::uint64_t hi = bindings_[i].connectionIdBase + stride_;
            if (lo > hi) {
                throw std::invalid_argument("ingress fan-in id range overflow");
            }
            for (std::size_t j = i + 1; j < bindings_.size(); ++j) {
                const std::uint64_t other_lo = bindings_[j].connectionIdBase + 1;
                const std::uint64_t other_hi = bindings_[j].connectionIdBase + stride_;
                if (lo <= other_hi && other_lo <= hi) {
                    throw std::invalid_argument(
                        "ingress fan-in id ranges overlap (bases " +
                        std::to_string(bindings_[i].connectionIdBase) + " and " +
                        std::to_string(bindings_[j].connectionIdBase) + ")");
                }
            }
        }
    }

    void start() override {
        std::vector<std::size_t> started;
        try {
            for (std::size_t i = 0; i < bindings_.size(); ++i) {
                bindings_[i].server->start();
                started.push_back(i);
            }
        } catch (...) {
            // 已启的子入口收干净再抛（半启动状态不留缝）。
            for (const auto i : started) {
                bindings_[i].server->stop();
            }
            throw;
        }
    }

    void stop() override {
        for (auto& binding : bindings_) {
            binding.server->stop();
        }
    }

    void pump() override {
        // 各子入口先泵后发事件：observer 回调在泵线程、重入 send/close 落
        // 子入口投递队列（下拍排空）。
        for (auto& binding : bindings_) {
            binding.server->pump();
        }
    }

    std::uint16_t listenPort() const override {
        // 首个子入口的监听端口（装配序即主入口序）。
        for (const auto& binding : bindings_) {
            const std::uint16_t port = binding.server->listenPort();
            if (port != 0) {
                return port;
            }
        }
        return 0;
    }

    std::vector<std::uint16_t> listenPorts() const override {
        std::vector<std::uint16_t> ports;
        ports.reserve(bindings_.size());
        for (const auto& binding : bindings_) {
            ports.push_back(binding.server->listenPort());
        }
        return ports;
    }

    void beginDrain() override {
        for (auto& binding : bindings_) {
            binding.server->beginDrain();
        }
    }

    std::size_t activeConnections() const override {
        std::size_t n = 0;
        for (const auto& binding : bindings_) {
            n += binding.server->activeConnections();
        }
        return n;
    }

    std::vector<ConnectionID> connectionIds() const override {
        std::vector<ConnectionID> ids;
        for (const auto& binding : bindings_) {
            const auto child_ids = binding.server->connectionIds();
            ids.insert(ids.end(), child_ids.begin(), child_ids.end());
        }
        return ids;
    }

    void send(ConnectionID connectionId, std::span<const std::uint8_t> payload) override {
        auto* child = route(connectionId);
        if (child != nullptr) {
            child->send(connectionId, payload);
        }
    }

    void close(ConnectionID connectionId, DisconnectReason reason) override {
        auto* child = route(connectionId);
        if (child != nullptr) {
            child->close(connectionId, reason);
        }
    }

    void setObserver(IClientIngressObserver* observer) override {
        // 观测面统一转发：子入口各自持有同一指针（回调仍在其泵线程发）。
        for (auto& binding : bindings_) {
            binding.server->setObserver(observer);
        }
    }

private:
    ClientIngressServer* route(ConnectionID connectionId) {
        if (connectionId == 0) {
            return nullptr;
        }
        for (auto& binding : bindings_) {
            const std::uint64_t base = binding.connectionIdBase;
            if (connectionId > base && connectionId <= base + stride_) {
                return binding.server.get();
            }
        }
        return nullptr;  // 越界号：迟到操作容忍（同 TCP 入口落地丢弃）
    }

    std::vector<IngressFanInBinding> bindings_;
    std::uint64_t stride_;
};

} // namespace

std::unique_ptr<ClientIngressServer> makeIngressFanIn(
    std::vector<IngressFanInBinding> bindings, std::uint64_t stride) {
    return std::make_unique<IngressFanIn>(std::move(bindings), stride);
}

} // namespace gateway
