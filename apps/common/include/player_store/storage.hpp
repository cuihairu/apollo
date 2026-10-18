#pragma once

#include <cstdint>
#include <vector>

namespace player_store {

struct PlayerData;

// Base depends on storage operations, not filesystem paths or database connections.
class PlayerStorage {
public:
    virtual ~PlayerStorage() = default;
    virtual bool initialize() = 0;
    virtual void shutdown() = 0;
    virtual bool loadPlayer(std::uint64_t playerId, PlayerData& out) = 0;
    virtual bool savePlayer(const PlayerData& data) = 0;
    virtual bool listPlayers(std::vector<std::uint64_t>& ids) = 0;
    // Remote storage owns its journal; Base must not replay a second storage log.
    virtual bool ownsJournal() const { return false; }
};

} // namespace player_store
