#pragma once

#include "apollo/bigworld/runtime.hpp"
#include <string>

namespace apollo::bigworld {

class Entity {
public:
    explicit Entity(EntityID id) : id_(id) {}

    EntityID id() const { return id_; }
    bool is_valid() const { return Runtime::entity_exists(id_); }

    template <typename T>
    void set(std::string_view name, const T& value);

    template <typename T>
    T get(std::string_view name, const T& default_value = T{});

private:
    EntityID id_;
};

} // namespace apollo::bigworld
