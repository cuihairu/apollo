#include "apollo/data/cache/primitive_cache.hpp"

namespace apollo::data::cache {

bool PrimitiveCache::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }

    if (is_expired(it->second)) {
        cache_.erase(it);
        return false;
    }

    value = it->second.value;
    return true;
}

void PrimitiveCache::set(const std::string& key, const std::string& value, int ttl_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    Entry entry;
    entry.value = value;

    if (ttl_seconds > 0) {
        entry.expiry = std::chrono::steady_clock::now() + std::chrono::seconds(ttl_seconds);
        entry.has_expiry = true;
    }

    cache_[key] = std::move(entry);
}

void PrimitiveCache::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    cache_.erase(key);
}

bool PrimitiveCache::exists(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return false;
    }

    if (is_expired(it->second)) {
        cache_.erase(it);
        return false;
    }

    return true;
}

void PrimitiveCache::cleanup_expired() {
    std::lock_guard<std::mutex> lock(mutex_);

    for (auto it = cache_.begin(); it != cache_.end(); ) {
        if (is_expired(it->second)) {
            it = cache_.erase(it);
        } else {
            ++it;
        }
    }
}

bool PrimitiveCache::is_expired(const Entry& entry) const {
    if (!entry.has_expiry) {
        return false;
    }
    return std::chrono::steady_clock::now() >= entry.expiry;
}

} // namespace apollo::data::cache
