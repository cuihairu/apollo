#include "database/server.hpp"
#include "player_store/database_service.hpp"
#include "player_store/m1_server.hpp"
#include "apollo/data/orm/persist_journal.hpp"
#include "apollo/protocol/codec.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <utility>

namespace database {
namespace protocol = apollo::protocol;

class Server::Impl {
public:
    explicit Impl(ServerConfig c)
        : config(std::move(c)), storage(config.storage), journal(config.storage.journalPath) {}

    bool apply(const apollo::data::journal::JournalEntry& entry) {
        try {
            auto profile = player_store::PlayerData::fromJson(entry.payload);
            if (!profile.playerId || std::to_string(profile.playerId) != entry.key) return false;
            return storage.savePlayer(profile);
        } catch (const std::exception&) { return false; }
    }
    bool drain() {
        journal.drain([this](const auto& entry) { return apply(entry); }, journal.pending());
        if (journal.pending()) return false;
        journal.compact();
        return true;
    }
    ServerConfig config;
    player_store::DatabaseService storage;
    apollo::data::journal::PersistJournal journal;
    std::unique_ptr<player_store::PlayerStoreLinkServer> listener;
    // All operations, including direct diagnostic/test calls, share one order.
    std::mutex operations;
    bool ready = false;
};

Server::Server(ServerConfig config) : impl_(std::make_unique<Impl>(std::move(config))) {}
Server::~Server() { stop(); }

bool Server::start() {
    if (impl_->listener) return true;
    const auto parent = std::filesystem::path(impl_->config.storage.journalPath).parent_path();
    std::error_code error;
    if (!parent.empty()) std::filesystem::create_directories(parent, error);
    if (error) return false;
    {
        std::ofstream probe(impl_->config.storage.journalPath, std::ios::app | std::ios::binary);
        if (!probe) return false;
    }
    if (!impl_->storage.initialize() || !impl_->journal.open() || !impl_->drain()) return false;
    player_store::PlayerStoreLinkServerConfig config;
    config.host = impl_->config.host;
    config.port = impl_->config.port;
    config.route_json = impl_->config.route_json;
    config.self_name = impl_->config.self_name;
    config.endpoint_name = "baseServer";
    impl_->ready = true;
    impl_->listener = player_store::makePlayerStoreLinkServer(config,
        [this](const std::uint8_t* bytes, std::size_t size) {
            return dispatch({bytes, bytes + size});
        });
    if (!impl_->listener->start()) {
        impl_->listener.reset();
        impl_->ready = false;
        return false;
    }
    return true;
}

void Server::stop() {
    if (impl_->listener) impl_->listener->stop();
    impl_->listener.reset();
    std::lock_guard<std::mutex> operation(impl_->operations);
    impl_->ready = false;
    impl_->storage.shutdown();
}

std::uint16_t Server::port() const {
    return impl_->listener ? impl_->listener->listenPort() : 0;
}

std::vector<std::uint8_t> Server::dispatch(const std::vector<std::uint8_t>& frame) {
    std::lock_guard<std::mutex> operation(impl_->operations);
    protocol::SessionID session = 0;
    try {
        const auto header = protocol::MessageCodec::parseHeader(frame);
        session = header.sessionId;
        if (!impl_->ready || header.version != protocol::MessageHeader::CURRENT_VERSION ||
            header.length != frame.size() - sizeof(header)) throw std::runtime_error("invalid storage request");
        const std::vector<std::uint8_t> body(frame.begin() + sizeof(header), frame.end());
        if (header.type == static_cast<std::uint16_t>(protocol::MessageType::DB_LOAD_REQUEST)) {
            const auto request = protocol::MessageCodec::decodeBody<protocol::DbLoadRequest>(body);
            protocol::DbLoadResponse response{};
            player_store::PlayerData profile;
            response.success = request.playerId && request.tableName == "players" &&
                               impl_->storage.loadPlayer(request.playerId, profile);
            if (response.success) response.jsonData = profile.toJson();
            else response.errorMessage = "profile unavailable";
            return protocol::MessageCodec::encode(response, session);
        }
        if (header.type == static_cast<std::uint16_t>(protocol::MessageType::DB_SAVE_REQUEST)) {
            const auto request = protocol::MessageCodec::decodeBody<protocol::DbSaveRequest>(body);
            protocol::DbSaveResponse response{};
            const auto profile = player_store::PlayerData::fromJson(request.jsonData);
            if (!request.playerId || request.tableName != "players" || profile.playerId != request.playerId) {
                response.errorMessage = "profile identity mismatch";
            } else if (!impl_->drain()) {
                response.errorMessage = "previous storage operation pending";
            } else {
                const auto now = std::chrono::system_clock::now().time_since_epoch();
                const auto sequence = impl_->journal.append(std::to_string(profile.playerId),
                    profile.toJson(), std::chrono::duration_cast<std::chrono::milliseconds>(now).count());
                // Success follows journal + profile application, never mere queue acceptance.
                response.success = sequence && impl_->drain();
                if (!response.success) response.errorMessage = "storage write failed";
            }
            return protocol::MessageCodec::encode(response, session);
        }
        throw std::runtime_error("unsupported storage message");
    } catch (const std::exception&) {
        protocol::ErrorMessage response{};
        response.code = 1;
        response.message = "storage request rejected";
        return protocol::MessageCodec::encode(response, session);
    }
}
} // namespace database
