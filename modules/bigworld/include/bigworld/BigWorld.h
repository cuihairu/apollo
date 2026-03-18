#pragma once

/**
 * @file BigWorld.h
 * @brief BigWorld API compatibility layer - provides legacy BigWorld namespace interface
 *
 * This header provides the BigWorld-compatible interface for legacy code migration.
 * New code should use apollo::bigworld namespace instead.
 */

#include "apollo/bigworld/runtime.hpp"
#include "apollo/bigworld/entity.hpp"

// Map legacy BigWorld namespace to apollo::bigworld
namespace BigWorld {
    using namespace apollo::bigworld;

    // Legacy BigWorld types
    using SpaceID = apollo::bigworld::SpaceID;
    using EntityID = apollo::bigworld::EntityID;

    // Legacy BigWorld Runtime functions
    inline double now() { return apollo::bigworld::Runtime::now(); }
    inline double deltaTime() { return apollo::bigworld::Runtime::delta_time(); }

    inline EntityID createEntity(const char* type) {
        return apollo::bigworld::Runtime::create_entity(type);
    }

    inline void destroyEntity(EntityID id) {
        apollo::bigworld::Runtime::destroy_entity(id);
    }

    inline bool entityExists(EntityID id) {
        return apollo::bigworld::Runtime::entity_exists(id);
    }

    inline SpaceID createSpace(const char* name) {
        return apollo::bigworld::Runtime::create_space(name);
    }

    inline void destroySpace(SpaceID id) {
        apollo::bigworld::Runtime::destroy_space(id);
    }

    // Initialization
    inline void initialize() { apollo::bigworld::Runtime::initialize(); }
    inline void shutdown() { apollo::bigworld::Runtime::shutdown(); }
    inline void update() { apollo::bigworld::Runtime::update(); }
}

// Convenience macros for BigWorld legacy code
#define BW_NOW BigWorld::now()
#define BW_DELTA_TIME BigWorld::deltaTime()
#define BW_CREATE_ENTITY(type) BigWorld::createEntity(type)
#define BW_DESTROY_ENTITY(id) BigWorld::destroyEntity(id)
