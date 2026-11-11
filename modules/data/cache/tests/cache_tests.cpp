#include "apollo/data/cache/cache_manager.hpp"
#include "apollo/data/cache/primitive_cache.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

bool test_primitive_cache_set_get_roundtrip() {
    apollo::data::cache::PrimitiveCache cache;

    cache.set("k1", "v1", 0);
    std::string value;
    if (!cache.get("k1", value) || value != "v1") {
        return false;
    }

    // 同键覆盖：后写胜
    cache.set("k1", "v2", 0);
    return cache.get("k1", value) && value == "v2";
}

bool test_primitive_cache_get_missing() {
    apollo::data::cache::PrimitiveCache cache;

    std::string value = "untouched";
    if (cache.get("nope", value)) {
        return false;
    }
    return value == "untouched";
}

bool test_primitive_cache_remove_and_exists() {
    apollo::data::cache::PrimitiveCache cache;

    cache.set("k", "v", 0);
    if (!cache.exists("k")) {
        return false;
    }

    cache.remove("k");
    std::string value;
    return !cache.exists("k") && !cache.get("k", value);
}

bool test_primitive_cache_ttl_expiry() {
    apollo::data::cache::PrimitiveCache cache;

    cache.set("temp", "t", 1);   // 1 秒后过期
    cache.set("perm", "p", 0);   // 永不过期
    if (!cache.exists("temp")) {
        return false;
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    std::string value;
    // 过期键：get/exists 均拒且惰性清除
    if (cache.get("temp", value) || cache.exists("temp")) {
        return false;
    }
    return cache.get("perm", value) && value == "p";
}

bool test_primitive_cache_ttl_reset_on_overwrite() {
    apollo::data::cache::PrimitiveCache cache;

    cache.set("k", "v1", 1);
    // 0 ttl 覆盖：has_expiry 复位，键转永不过期
    cache.set("k", "v2", 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    std::string value;
    return cache.get("k", value) && value == "v2";
}

bool test_primitive_cache_cleanup_expired() {
    apollo::data::cache::PrimitiveCache cache;

    cache.set("gone", "g", 1);
    cache.set("stay", "s", 0);

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));
    cache.cleanup_expired();

    std::string value;
    if (cache.exists("gone")) {
        return false;
    }
    return cache.get("stay", value) && value == "s";
}

bool test_cache_manager_without_provider() {
    auto& manager = apollo::data::cache::CacheManager::instance();
    manager.set_provider(nullptr);  // 单例跨用例复位，保序独立

    // 无 provider：读恒 false，写为无操作（不崩溃）
    std::string value;
    if (manager.get("k", value) || manager.exists("k")) {
        return false;
    }
    manager.set("k", "v", 0);
    manager.remove("k");
    return !manager.exists("k");
}

bool test_cache_manager_with_provider() {
    auto provider = std::make_shared<apollo::data::cache::PrimitiveCache>();
    auto& manager = apollo::data::cache::CacheManager::instance();
    manager.set_provider(provider);

    manager.set("session", "abc", 10);
    std::string value;
    bool ok = manager.get("session", value) && value == "abc";
    ok = ok && manager.exists("session");

    manager.remove("session");
    ok = ok && !manager.exists("session") && !manager.get("session", value);

    // 复原单例 provider，避免污染其他进程内用例
    manager.set_provider(nullptr);
    return ok;
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"set_get_roundtrip", test_primitive_cache_set_get_roundtrip},
        {"get_missing", test_primitive_cache_get_missing},
        {"remove_and_exists", test_primitive_cache_remove_and_exists},
        {"ttl_expiry", test_primitive_cache_ttl_expiry},
        {"ttl_reset_on_overwrite", test_primitive_cache_ttl_reset_on_overwrite},
        {"cleanup_expired", test_primitive_cache_cleanup_expired},
        {"manager_without_provider", test_cache_manager_without_provider},
        {"manager_with_provider", test_cache_manager_with_provider},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... ";
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "apollo_data_cache_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "apollo_data_cache_tests passed" << std::endl;
    return 0;
}
