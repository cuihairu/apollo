#pragma once

#include <string_view>

namespace apollo::runtime {

struct RuntimeManifest {
    std::string_view name;
    std::string_view layer;
    std::string_view description;
};

const RuntimeManifest& runtime_manifest();

} // namespace apollo::runtime
