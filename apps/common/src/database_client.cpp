#include "database/client.hpp"
#include "player_store/database_service.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"

#include <atomic>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>

namespace database {
namespace {
using namespace std::chrono_literals;
namespace protocol = apollo::protocol;

class Client final : public player_store::PlayerStorage {
public:
    explicit Client(ClientConfig config) : config_(std::move(config)) {}
    ~Client() override { shutdown(); }

    bool initialize() override {
#if !defined(_WIN32)
        std::lock_guard<std::mutex> operation(operationMutex_);
        if (running_) return ready_.load();
        std::ifstream in(config_.route_json);
        std::ostringstream json;
        json << in.rdbuf();
        routes_ = std::make_unique<apollo::net::link::RouteTable>();
#if APOLLO_NET_LINK_HAS_JSON
        if (!in || !apollo::net::link::load_route_table_json(json.str(), *routes_)) return false;
#else
        return false;
#endif
        apollo::net::link::LinkManagerConfig mc;
        mc.self_name = config_.self_name;
        mc.route_version = apollo::net::link::route_version_from_hash(routes_->schema_hash());
        mc.compat_route_versions = apollo::net::link::compat_route_versions_from_env();
        manager_ = std::make_unique<apollo::net::link::LinkManager>(mc);
        apollo::net::link::EndpointConfig endpoint;
        endpoint.name = config_.peer_name;
        endpoint.host = config_.host;
        endpoint.port = config_.port;
        endpoint.role = apollo::net::link::EndpointRole::kActive;
        if (!manager_->add_endpoint(endpoint)) return false;
        auto* peer = manager_->link(endpoint.name);
        peer->dispatcher().set_route_table(routes_.get());
        peer->dispatcher().allow_domain("internal");
        rpc_ = std::make_unique<apollo::net::linkrpc::RpcClient>(*peer);
        running_ = true;
        pump_ = std::thread([this] {
            while (running_) {
                {
                    // LinkManager and InterServerLink are single-writer primitives.
                    std::lock_guard<std::mutex> network(networkMutex_);
                    const auto now = std::chrono::steady_clock::now().time_since_epoch();
                    manager_->pump(std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
                    auto* peer = manager_->link(config_.peer_name);
                    ready_ = peer && peer->state() == apollo::net::link::LinkState::kUp;
                }
                std::this_thread::sleep_for(2ms);
            }
        });
        const auto deadline = std::chrono::steady_clock::now() + config_.timeout;
        while (!ready_ && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(2ms);
        }
        if (!ready_) stopUnlocked();
        return ready_;
#else
        return false;
#endif
    }

    void shutdown() override {
#if !defined(_WIN32)
        std::lock_guard<std::mutex> operation(operationMutex_);
        stopUnlocked();
#endif
    }

    bool loadPlayer(std::uint64_t id, player_store::PlayerData& out) override {
        protocol::DbLoadRequest request;
        request.playerId = id;
        request.tableName = "players";
        protocol::DbLoadResponse response;
        if (!exchange(request, response) || !response.success) return false;
        try {
            auto data = player_store::PlayerData::fromJson(response.jsonData);
            if (data.playerId != id) return false;
            out = std::move(data);
            return true;
        } catch (const std::exception&) { return false; }
    }

    bool savePlayer(const player_store::PlayerData& data) override {
        protocol::DbSaveRequest request;
        request.playerId = data.playerId;
        request.tableName = "players";
        request.jsonData = data.toJson();
        protocol::DbSaveResponse response;
        return exchange(request, response) && response.success;
    }

    bool listPlayers(std::vector<std::uint64_t>& ids) override {
        // Base restores players on demand. It must not enumerate another service's
        // files or claim all players in a database shared by several Bases.
        ids.clear();
        return true;
    }
    bool ownsJournal() const override { return true; }

private:
    template<class Request, class Response>
    bool exchange(const Request& request, Response& response) {
#if !defined(_WIN32)
        std::lock_guard<std::mutex> operation(operationMutex_);
        if (!running_ || !ready_) return false;
        std::uint64_t correlation;
        {
            std::lock_guard<std::mutex> network(networkMutex_);
            correlation = rpc_->call_async(protocol::MessageCodec::encode(request));
        }
        if (!correlation) return false;
        const auto reply = rpc_->wait(correlation, config_.timeout);
        if (!reply) return false; // Ambiguous writes are not automatically replayed.
        try {
            const auto header = protocol::MessageCodec::parseHeader(*reply);
            if (header.version != protocol::MessageHeader::CURRENT_VERSION ||
                header.type != static_cast<std::uint16_t>(Response::TYPE) ||
                header.length != reply->size() - sizeof(header)) return false;
            response = protocol::MessageCodec::decodeBody<Response>(
                {reply->begin() + sizeof(header), reply->end()});
            return true;
        } catch (const std::exception&) { return false; }
#else
        (void)request; (void)response;
        return false;
#endif
    }
#if !defined(_WIN32)
    void stopUnlocked() {
        running_ = false;
        if (pump_.joinable()) pump_.join();
        ready_ = false;
        rpc_.reset();
        manager_.reset();
        routes_.reset();
    }
    std::unique_ptr<apollo::net::link::RouteTable> routes_;
    std::unique_ptr<apollo::net::link::LinkManager> manager_;
    std::unique_ptr<apollo::net::linkrpc::RpcClient> rpc_;
    std::thread pump_;
    std::mutex operationMutex_, networkMutex_;
    std::atomic<bool> running_{false}, ready_{false};
#endif
    ClientConfig config_;
};
} // namespace

std::unique_ptr<player_store::PlayerStorage> makeClient(ClientConfig config) {
    return std::make_unique<Client>(std::move(config));
}
} // namespace database
