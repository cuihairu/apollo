#include "apollo/runtime/runtime_manifest.hpp"

namespace apollo::runtime {

const RuntimeManifest& runtime_manifest() {
    static const RuntimeManifest manifest{
        "apollo_runtime",
        "runtime",
        "Apollo process host: service host, tick loop, signals, console events"
    };
    return manifest;
}

} // namespace apollo::runtime
