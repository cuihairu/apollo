#pragma once

#include "apollo/data/cache/cache_manager.hpp"
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace apollo::data::cache {

class PrimitiveCache : public ICacheProvider {
public:
    struct Entry {
        std::string value;
        std::chrono::steady_clock::time_point expiry;
        bool has_expiry = false;
    };

    bool get(const std::string& key, std::string& value) override;
    void set(const std::string& key, const std::string& value, int ttl_seconds) override;
    void remove(const std::string& key) override;
    bool exists(const std::string& key) override;

    void cleanup_expired();

private:
    bool is_expired(const Entry& entry) const;

    std::unordered_map<std::string, Entry> cache_;
    mutable std::mutex mutex_;
};

} // namespace apollo::data::cache
