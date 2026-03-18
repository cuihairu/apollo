#pragma once

#include <memory>
#include <string>
#include <functional>

namespace apollo::data::cache {

class ICacheProvider {
public:
    virtual ~ICacheProvider() = default;
    virtual bool get(const std::string& key, std::string& value) = 0;
    virtual void set(const std::string& key, const std::string& value, int ttl_seconds) = 0;
    virtual void remove(const std::string& key) = 0;
    virtual bool exists(const std::string& key) = 0;
};

class CacheManager {
public:
    static CacheManager& instance();

    void set_provider(std::shared_ptr<ICacheProvider> provider);

    bool get(const std::string& key, std::string& value);
    void set(const std::string& key, const std::string& value, int ttl_seconds = 0);
    void remove(const std::string& key);
    bool exists(const std::string& key);

    template <typename T>
    T get_or_compute(const std::string& key, std::function<T()> compute, int ttl_seconds = 0);

private:
    CacheManager() = default;
    std::shared_ptr<ICacheProvider> provider_;
};

} // namespace apollo::data::cache
