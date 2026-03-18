#include "apollo/data/cache/cache_manager.hpp"

namespace apollo::data::cache {

CacheManager& CacheManager::instance() {
    static CacheManager instance;
    return instance;
}

void CacheManager::set_provider(std::shared_ptr<ICacheProvider> provider) {
    provider_ = std::move(provider);
}

bool CacheManager::get(const std::string& key, std::string& value) {
    if (provider_) {
        return provider_->get(key, value);
    }
    return false;
}

void CacheManager::set(const std::string& key, const std::string& value, int ttl_seconds) {
    if (provider_) {
        provider_->set(key, value, ttl_seconds);
    }
}

void CacheManager::remove(const std::string& key) {
    if (provider_) {
        provider_->remove(key);
    }
}

bool CacheManager::exists(const std::string& key) {
    if (provider_) {
        return provider_->exists(key);
    }
    return false;
}

} // namespace apollo::data::cache
