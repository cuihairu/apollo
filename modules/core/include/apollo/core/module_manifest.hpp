#pragma once

#include <string_view>

namespace apollo::core {

struct ModuleManifest {
    std::string_view name;
    std::string_view layer;
    std::string_view description;
};

const ModuleManifest& core_manifest();

} // namespace apollo::core
