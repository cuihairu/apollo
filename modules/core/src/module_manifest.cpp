#include "apollo/core/module_manifest.hpp"

namespace apollo::core {

const ModuleManifest& core_manifest() {
    static const ModuleManifest manifest{
        "apollo_core",
        "core",
        "Apollo framework kernel: config, log, DI, lifecycle, events"
    };
    return manifest;
}

} // namespace apollo::core
