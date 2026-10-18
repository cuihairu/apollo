#pragma once

#include "player_store/config.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace database {

struct ServerConfig {
    player_store::PlayerStoreConfig storage;
    std::string host = "127.0.0.1";
    std::uint16_t port = 9200;
    std::string route_json;
    std::string self_name = "database";
};

// Dedicated storage service: no Anchor, Avatar, or reward rules.
class Server {
public:
    explicit Server(ServerConfig config);
    ~Server();
    bool start();
    void stop();
    std::uint16_t port() const;
    std::vector<std::uint8_t> dispatch(const std::vector<std::uint8_t>& frame);
private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace database
