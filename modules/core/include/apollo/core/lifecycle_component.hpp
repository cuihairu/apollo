#pragma once

#include "apollo/core/application_lifecycle.hpp"

#include <string_view>

namespace apollo::core {

class LifecycleComponent : public IApplicationLifecycle {
public:
    virtual ~LifecycleComponent() = default;
    virtual std::string_view name() const = 0;
};

} // namespace apollo::core
