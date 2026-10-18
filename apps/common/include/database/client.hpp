#pragma once

#include "player_store/storage.hpp"
#include <chrono>
#include <memory>
#include <string>

namespace database {

struct ClientConfig {
    std::string host = "127.0.0.1";
    std::uint16_t port = 9200;
    std::string route_json;
    std::string self_name = "base";
    std::string peer_name = "database";
    std::chrono::milliseconds timeout{3000};
};

// No local storage fallback. initialize() succeeds only after the database handshake.
std::unique_ptr<player_store::PlayerStorage> makeClient(ClientConfig config);

} // namespace database
