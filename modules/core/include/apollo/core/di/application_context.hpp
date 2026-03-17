#pragma once

#include "apollo/core/di/type_key.hpp"
#include "apollo/core/di/unique_bean.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace apollo::core::di {

enum class BeanScope : uint8_t {
    Singleton = 0,
    Prototype = 1,
};

class ApplicationContext;

using CreateFn = void* (*)(ApplicationContext& ctx);
using CastFn = void* (*)(void* impl_ptr);

namespace detail {

struct TypeKeyHash {
    size_t operator()(TypeKey key) const noexcept {
        return std::hash<const void*>()(key);
    }
};

template <typename Impl, typename... Deps>
void* create_bean(ApplicationContext& ctx);

template <typename Impl>
void destroy_bean(void* ptr);

template <typename Impl, typename Base>
void* cast_bean(void* impl_ptr);

struct Exposure {
    TypeKey type = nullptr;
    CastFn cast = nullptr;
};

struct BeanDefinition {
    TypeKey impl_type = nullptr;
    CreateFn create = nullptr;
    DestroyFn destroy = nullptr;
    BeanScope scope = BeanScope::Singleton;
    bool eager = true;
    std::string name;
    std::vector<std::string> tags;
    std::vector<TypeKey> ctor_deps;
    std::vector<TypeKey> depends_on;
    std::vector<Exposure> exposes;
};

} // namespace detail

class ApplicationContextBuilder {
public:
    ApplicationContextBuilder() = default;

    ApplicationContextBuilder(const ApplicationContextBuilder&) = delete;
    ApplicationContextBuilder& operator=(const ApplicationContextBuilder&) = delete;

    template <typename Impl, typename... Deps>
    class BeanBuilder;

    template <typename Impl, typename... Deps>
    BeanBuilder<Impl, Deps...> add_singleton();

    template <typename Impl, typename... Deps>
    BeanBuilder<Impl, Deps...> add_prototype();

    ApplicationContext build();

private:
    template <typename Impl, typename... Deps>
    size_t add_bean_definition(BeanScope scope);

    std::vector<detail::BeanDefinition> definitions_;

    template <typename Impl, typename... Deps>
    friend class BeanBuilder;
};

template <typename Impl, typename... Deps>
class ApplicationContextBuilder::BeanBuilder {
public:
    BeanBuilder(ApplicationContextBuilder& owner, size_t bean_index)
        : owner_(owner), bean_index_(bean_index) {}

    BeanBuilder& name(const char* value) {
        owner_.definitions_[bean_index_].name = value ? value : "";
        return *this;
    }

    BeanBuilder& tag(const char* value) {
        if (value != nullptr && *value != '\0') {
            owner_.definitions_[bean_index_].tags.emplace_back(value);
        }
        return *this;
    }

    BeanBuilder& eager(bool enabled = true) {
        owner_.definitions_[bean_index_].eager = enabled;
        return *this;
    }

    template <typename Base>
    BeanBuilder& as() {
        static_assert(std::is_base_of<Base, Impl>::value, "Base must be a base of Impl");
        owner_.definitions_[bean_index_].exposes.push_back(
            {type_key_of<Base>(), &detail::cast_bean<Impl, Base>});
        return *this;
    }

    template <typename Dep>
    BeanBuilder& depends_on() {
        owner_.definitions_[bean_index_].depends_on.push_back(type_key_of<Dep>());
        return *this;
    }

private:
    ApplicationContextBuilder& owner_;
    size_t bean_index_ = 0;
};

class ApplicationContext {
public:
    ApplicationContext() = default;
    ~ApplicationContext();

    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

    ApplicationContext(ApplicationContext&&) noexcept = default;
    ApplicationContext& operator=(ApplicationContext&&) noexcept = default;

    bool initialize();
    void shutdown();

    template <typename T>
    T& get();

    template <typename T>
    T* try_get();

    template <typename T>
    T& get_named(const char* name);

    template <typename T>
    T* try_get_named(const char* name);

    template <typename T>
    std::vector<T*> get_all();

    template <typename T>
    UniqueBean<T> create();

private:
    struct ExposureBinding {
        size_t bean_index = 0;
        CastFn cast = nullptr;
    };

    struct AnyDeleter {
        DestroyFn destroy = nullptr;
        void operator()(void* ptr) const noexcept {
            if (ptr != nullptr && destroy != nullptr) {
                destroy(ptr);
            }
        }
    };

    using AnyUniquePtr = std::unique_ptr<void, AnyDeleter>;

    struct BeanRuntime {
        AnyUniquePtr instance = AnyUniquePtr(nullptr, AnyDeleter{});
    };

    using TypeMap = std::unordered_map<TypeKey, std::vector<ExposureBinding>, detail::TypeKeyHash>;

    explicit ApplicationContext(std::vector<detail::BeanDefinition> definitions);

    bool build_index();
    bool build_init_order();
    bool ensure_singleton_created(size_t bean_index);

    std::vector<detail::BeanDefinition> definitions_;
    std::vector<BeanRuntime> singletons_;
    TypeMap by_type_;
    std::unordered_map<std::string, size_t> by_name_;
    std::vector<size_t> init_order_;
    bool initialized_ = false;

    friend class ApplicationContextBuilder;
};

template <typename Impl, typename... Deps>
size_t ApplicationContextBuilder::add_bean_definition(BeanScope scope) {
    detail::BeanDefinition definition;
    definition.impl_type = type_key_of<Impl>();
    definition.create = &detail::create_bean<Impl, Deps...>;
    definition.destroy = &detail::destroy_bean<Impl>;
    definition.scope = scope;
    definition.eager = (scope == BeanScope::Singleton);
    definition.exposes.push_back({type_key_of<Impl>(), &detail::cast_bean<Impl, Impl>});
    definition.ctor_deps = {type_key_of<Deps>()...};
    definitions_.push_back(std::move(definition));
    return definitions_.size() - 1;
}

template <typename Impl, typename... Deps>
auto ApplicationContextBuilder::add_singleton() -> BeanBuilder<Impl, Deps...> {
    return BeanBuilder<Impl, Deps...>(*this, add_bean_definition<Impl, Deps...>(BeanScope::Singleton));
}

template <typename Impl, typename... Deps>
auto ApplicationContextBuilder::add_prototype() -> BeanBuilder<Impl, Deps...> {
    const auto index = add_bean_definition<Impl, Deps...>(BeanScope::Prototype);
    definitions_[index].eager = false;
    return BeanBuilder<Impl, Deps...>(*this, index);
}

template <typename T>
T& ApplicationContext::get() {
    T* ptr = try_get<T>();
    assert(ptr != nullptr);
    return *ptr;
}

template <typename T>
T* ApplicationContext::try_get() {
    const TypeKey key = type_key_of<T>();
    auto it = by_type_.find(key);
    if (it == by_type_.end() || it->second.empty()) {
        return nullptr;
    }
    if (it->second.size() != 1) {
        assert(false && "try_get<T>() ambiguous");
        return nullptr;
    }
    const auto& binding = it->second[0];
    if (!ensure_singleton_created(binding.bean_index)) {
        return nullptr;
    }
    void* impl_ptr = singletons_[binding.bean_index].instance.get();
    return static_cast<T*>(binding.cast(impl_ptr));
}

template <typename T>
T& ApplicationContext::get_named(const char* name) {
    T* ptr = try_get_named<T>(name);
    assert(ptr != nullptr);
    return *ptr;
}

template <typename T>
T* ApplicationContext::try_get_named(const char* name) {
    if (name == nullptr || *name == '\0') {
        return nullptr;
    }
    auto it = by_name_.find(name);
    if (it == by_name_.end()) {
        return nullptr;
    }
    const size_t bean_index = it->second;
    if (!ensure_singleton_created(bean_index)) {
        return nullptr;
    }
    const TypeKey key = type_key_of<T>();
    const auto& definition = definitions_[bean_index];
    for (const auto& exposure : definition.exposes) {
        if (exposure.type == key) {
            void* impl_ptr = singletons_[bean_index].instance.get();
            return static_cast<T*>(exposure.cast(impl_ptr));
        }
    }
    return nullptr;
}

template <typename T>
std::vector<T*> ApplicationContext::get_all() {
    std::vector<T*> out;
    const TypeKey key = type_key_of<T>();
    auto it = by_type_.find(key);
    if (it == by_type_.end()) {
        return out;
    }
    out.reserve(it->second.size());
    for (const auto& binding : it->second) {
        if (!ensure_singleton_created(binding.bean_index)) {
            continue;
        }
        void* impl_ptr = singletons_[binding.bean_index].instance.get();
        out.push_back(static_cast<T*>(binding.cast(impl_ptr)));
    }
    return out;
}

template <typename T>
UniqueBean<T> ApplicationContext::create() {
    const TypeKey key = type_key_of<T>();
    auto it = by_type_.find(key);
    if (it == by_type_.end() || it->second.empty()) {
        return {};
    }
    if (it->second.size() != 1) {
        assert(false && "create<T>() ambiguous");
        return {};
    }
    const auto& binding = it->second[0];
    const auto& definition = definitions_[binding.bean_index];
    if (definition.scope != BeanScope::Prototype) {
        assert(false && "create<T>() requires prototype scope");
        return {};
    }
    void* impl_ptr = definition.create(*this);
    if (impl_ptr == nullptr) {
        return {};
    }
    T* view_ptr = static_cast<T*>(binding.cast(impl_ptr));
    return UniqueBean<T>(impl_ptr, view_ptr, definition.destroy);
}

namespace detail {

template <typename Impl, typename... Deps>
void* create_bean(ApplicationContext& ctx) {
    return new Impl(ctx.get<Deps>()...);
}

template <typename Impl>
void destroy_bean(void* ptr) {
    delete static_cast<Impl*>(ptr);
}

template <typename Impl, typename Base>
void* cast_bean(void* impl_ptr) {
    return static_cast<Base*>(static_cast<Impl*>(impl_ptr));
}

} // namespace detail

} // namespace apollo::core::di
