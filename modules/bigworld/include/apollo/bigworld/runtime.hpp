#pragma once

#include "apollo/bw/runtime.h"
#include "apollo/bw/entity.h"

// Export the existing apollo::bw namespace as apollo::bigworld for clarity
namespace apollo::bigworld {
    using namespace apollo::bw;

    // Re-export core types with clearer names
    using BWRuntime = Runtime;
    using BWEntity = Entity;
    using BWBigWorld = BigWorld;
}

// Convenience macros
#define BW_RUNTIME apollo::bigworld::BigWorld::instance()
#define BW_NOW() apollo::bigworld::BigWorld::time()
#define BW_UPDATE() apollo::bigworld::BigWorld::update()
