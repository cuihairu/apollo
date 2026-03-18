#include "apollo/core/di/application_context.hpp"

namespace apollo::core::di {

ApplicationContext::ApplicationContext(std::vector<detail::BeanDefinition> definitions)
    : definitions_(std::move(definitions)) {}

ApplicationContext::~ApplicationContext() {
    shutdown();
}

ApplicationContext ApplicationContextBuilder::build() {
    ApplicationContext ctx(std::move(definitions_));
    const bool index_ok = ctx.build_index();
    assert(index_ok);
    const bool order_ok = ctx.build_init_order();
    assert(order_ok);
    return ctx;
}

bool ApplicationContext::initialize() {
    if (initialized_) {
        return true;
    }
    initialized_ = true;

    for (size_t index : init_order_) {
        const auto& definition = definitions_[index];
        if (definition.scope != BeanScope::Singleton || !definition.eager) {
            continue;
        }
        if (!ensure_singleton_created(index)) {
            return false;
        }
    }
    return true;
}

void ApplicationContext::shutdown() {
    if (!initialized_) {
        return;
    }
    initialized_ = false;

    for (auto it = init_order_.rbegin(); it != init_order_.rend(); ++it) {
        const size_t index = *it;
        if (index >= singletons_.size()) {
            continue;
        }
        singletons_[index].instance.reset();
    }
}

bool ApplicationContext::build_index() {
    by_type_.clear();
    by_name_.clear();
    init_order_.clear();
    singletons_.clear();

    singletons_.resize(definitions_.size());
    by_type_.reserve(definitions_.size() * 2);
    by_name_.reserve(definitions_.size());

    for (size_t i = 0; i < definitions_.size(); ++i) {
        const auto& definition = definitions_[i];

        if (!definition.name.empty()) {
            const auto [_, inserted] = by_name_.emplace(definition.name, i);
            assert(inserted && "Bean name must be unique");
            if (!inserted) {
                return false;
            }
        }

        for (const auto& exposure : definition.exposes) {
            by_type_[exposure.type].push_back({i, exposure.cast});
        }
    }

    for (size_t i = 0; i < definitions_.size(); ++i) {
        const auto& definition = definitions_[i];
        if (definition.scope != BeanScope::Singleton) {
            continue;
        }

        for (TypeKey dep : definition.ctor_deps) {
            auto it = by_type_.find(dep);
            assert(it != by_type_.end() && it->second.size() == 1);
            if (it == by_type_.end() || it->second.size() != 1) {
                return false;
            }
            const size_t dep_index = it->second[0].bean_index;
            assert(definitions_[dep_index].scope == BeanScope::Singleton);
            if (definitions_[dep_index].scope != BeanScope::Singleton) {
                return false;
            }
        }
    }

    return true;
}

bool ApplicationContext::build_init_order() {
    init_order_.clear();
    init_order_.reserve(definitions_.size());

    const size_t n = definitions_.size();
    std::vector<int> indegree(n, 0);
    std::vector<std::vector<size_t>> adjacency(n);

    auto add_edge = [&](size_t from, size_t to) {
        adjacency[from].push_back(to);
        ++indegree[to];
    };

    for (size_t i = 0; i < n; ++i) {
        const auto& definition = definitions_[i];
        if (definition.scope != BeanScope::Singleton) {
            continue;
        }

        auto add_deps = [&](const std::vector<TypeKey>& deps) {
            for (TypeKey dep : deps) {
                auto it = by_type_.find(dep);
                assert(it != by_type_.end() && it->second.size() == 1);
                if (it == by_type_.end() || it->second.size() != 1) {
                    continue;
                }
                const size_t dep_index = it->second[0].bean_index;
                if (definitions_[dep_index].scope != BeanScope::Singleton) {
                    continue;
                }
                add_edge(dep_index, i);
            }
        };

        add_deps(definition.ctor_deps);
        add_deps(definition.depends_on);
    }

    std::deque<size_t> queue;
    for (size_t i = 0; i < n; ++i) {
        if (definitions_[i].scope == BeanScope::Singleton && indegree[i] == 0) {
            queue.push_back(i);
        }
    }

    while (!queue.empty()) {
        const size_t current = queue.front();
        queue.pop_front();
        init_order_.push_back(current);

        for (size_t next : adjacency[current]) {
            if (--indegree[next] == 0) {
                queue.push_back(next);
            }
        }
    }

    size_t singleton_count = 0;
    for (const auto& definition : definitions_) {
        if (definition.scope == BeanScope::Singleton) {
            ++singleton_count;
        }
    }

    assert(init_order_.size() == singleton_count && "Singleton dependency cycle detected");
    return init_order_.size() == singleton_count;
}

bool ApplicationContext::ensure_singleton_created(size_t bean_index) {
    if (bean_index >= definitions_.size()) {
        return false;
    }

    const auto& definition = definitions_[bean_index];
    if (definition.scope != BeanScope::Singleton) {
        return false;
    }

    BeanRuntime& runtime = singletons_[bean_index];
    if (runtime.instance) {
        return true;
    }

    void* impl_ptr = definition.create(*this);
    if (impl_ptr == nullptr) {
        return false;
    }

    runtime.instance = AnyUniquePtr(impl_ptr, AnyDeleter{definition.destroy});
    return true;
}

} // namespace apollo::core::di
