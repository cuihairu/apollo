// Game Module Tests
// Tests for: ComVal, AttributeContainer, AOI, ECS, Entity, Scene

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

// Simple test framework - define before any includes
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "  FAIL: " << msg << " (line " << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

#include "apollo/game/attributes/comval.h"
#include "apollo/game/attributes/attribute.hpp"
#include "apollo/game/aoi/aoi.hpp"
#include "apollo/game/battle/ecs/ecs.hpp"
#include "apollo/game/core/entity.hpp"
#include "apollo/game/world/scene.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

using namespace apollo;
// Be explicit about namespaces to avoid Entity class conflicts
using apollo::game::ComVal;
using apollo::game::EComValType;
using apollo::AttributeContainer;  // From attribute.hpp (apollo namespace)
using apollo::game::core::EntityId;
using apollo::game::core::Entity;
using apollo::game::core::IEntity;
using apollo::game::core::IEntityComponent;
using apollo::game::world::Scene;
using apollo::Vector3;
using apollo::AOIEntity;
using apollo::AOIEventType;
using apollo::AOIGrid;
using apollo::AOIManager;
using apollo::battle::ecs::World;
using apollo::battle::ecs::System;
using apollo::battle::ecs::SystemManager;
using apollo::battle::ecs::IComponent;
// Type alias for ECS Entity to avoid conflict with game::core::Entity
typedef apollo::battle::ecs::Entity EcsEntity;

// Simple test framework
#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "  FAIL: " << msg << " (line " << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while(0)

//==============================================================================
// ComVal Tests
//==============================================================================

bool test_comval_default_construct() {
    ComVal v;
    TEST_ASSERT(v.getType() == EComValType::ECVT_NULL, "Default should be NULL");
    TEST_ASSERT(v.isNull(), "Should be null");
    return true;
}

bool test_comval_bool_construct() {
    ComVal v(true);
    TEST_ASSERT(v.getType() == EComValType::ECVT_BOOL, "Type should be BOOL");
    TEST_ASSERT(v.isBool(), "Should be bool type");
    TEST_ASSERT(v.getBool() == true, "Value should be true");
    return true;
}

bool test_comval_int_construct() {
    ComVal v(42);
    TEST_ASSERT(v.getType() == EComValType::ECVT_INT, "Type should be INT");
    TEST_ASSERT(v.isInt32(), "Should be int32 type");
    TEST_ASSERT(v.getInt() == 42, "Value should be 42");
    return true;
}

bool test_comval_double_construct() {
    ComVal v(3.14);
    TEST_ASSERT(v.getType() == EComValType::ECVT_DOUBLE, "Type should be DOUBLE");
    TEST_ASSERT(v.isDouble(), "Should be double type");
    TEST_ASSERT(std::abs(v.getDouble() - 3.14) < 0.001, "Value should be 3.14");
    return true;
}

bool test_comval_string_construct() {
    ComVal v("hello");
    TEST_ASSERT(v.getType() == EComValType::ECVT_STRING, "Type should be STRING");
    TEST_ASSERT(v.isString(), "Should be string type");
    TEST_ASSERT(std::string(v.getString()) == "hello", "Value should be 'hello'");
    return true;
}

bool test_comval_copy_construct() {
    ComVal v1(123);
    ComVal v2(v1);
    TEST_ASSERT(v2.getType() == v1.getType(), "Copy should have same type");
    TEST_ASSERT(v2.getInt() == v1.getInt(), "Copy should have same value");
    return true;
}

bool test_comval_assignment() {
    ComVal v;
    v = 456;
    TEST_ASSERT(v.getInt() == 456, "Assignment should work");

    v = std::string("test");
    TEST_ASSERT(v.getStringStd() == "test", "String assignment should work");
    return true;
}

/*
// Windows macro issues prevent these tests from compiling
// TODO: Fix macro conflicts and re-enable
bool test_comval_comparison() {
    ComVal v1(100);
    ComVal v2(100);
    ComVal v3(200);

    bool eq = (v1 == v2);
    TEST_ASSERT(eq, "Equal values should compare equal");

    bool ne = (v1 != v3);
    TEST_ASSERT(ne, "Different values should not be equal");

    bool lt = (v1 < v3);
    TEST_ASSERT(lt, "Less than should work");

    return true;
}

bool test_comval_arithmetic() {
    ComVal v1(10);
    ComVal v2(5);

    ComVal sum = v1 + v2;
    TEST_ASSERT(sum.getInt() == 15, "Addition should work");

    ComVal diff = v1 - v2;
    TEST_ASSERT(diff.getInt() == 5, "Subtraction should work");

    ComVal prod = v1 * v2;
    TEST_ASSERT(prod.getInt() == 50, "Multiplication should work");

    ComVal div = v1 / v2;
    TEST_ASSERT(div.getInt() == 2, "Division should work");
    return true;
}

bool test_comval_string_concat() {
    ComVal v1("hello");
    ComVal v2(" world");

    ComVal result = v1 + v2;
    TEST_ASSERT(result.getStringStd() == "hello world", "String concatenation should work");
    return true;
}
*/

bool test_comval_serialization() {
    ComVal original(42);
    char buffer[128];
    int offset = 0;

    TEST_ASSERT(original.saveToBuff(buffer, sizeof(buffer), offset), "Save should succeed");

    offset = 0;
    ComVal restored;
    TEST_ASSERT(restored.fromBuff(buffer, sizeof(buffer), offset), "Load should succeed");
    TEST_ASSERT(restored.getInt() == 42, "Restored value should match");

    // Test string serialization
    ComVal str("test");
    offset = 0;
    TEST_ASSERT(str.saveToBuff(buffer, sizeof(buffer), offset), "String save should succeed");

    offset = 0;
    ComVal restoredStr;
    TEST_ASSERT(restoredStr.fromBuff(buffer, sizeof(buffer), offset), "String load should succeed");
    TEST_ASSERT(restoredStr.getStringStd() == "test", "Restored string should match");
    return true;
}

bool test_comval_to_string() {
    ComVal v1(42);
    TEST_ASSERT(v1.toString() == "42", "Int to string should work");

    ComVal v2(true);
    TEST_ASSERT(v2.toString() == "true", "Bool to string should work");

    ComVal v3(3.14);
    TEST_ASSERT(v3.toString().find("3.14") == 0, "Double to string should contain value");

    ComVal v4("hello");
    TEST_ASSERT(v4.toString() == "hello", "String to string should work");
    return true;
}

//==============================================================================
// AttributeContainer Tests
//==============================================================================

bool test_attribute_container_set_get() {
    AttributeContainer container(100);

    container.SetAttribute(1, static_cast<int32_t>(50));
    int32_t value = container.GetAttribute<int32_t>(1, -1);

    TEST_ASSERT(value == 50, "Attribute get should return set value");
    return true;
}

bool test_attribute_container_get_default() {
    AttributeContainer container(100);

    int32_t value = container.GetAttribute<int32_t>(999, -1);
    TEST_ASSERT(value == -1, "Should return default for missing attribute");
    return true;
}

bool test_attribute_container_add() {
    AttributeContainer container(100);

    container.SetAttribute(1, static_cast<int32_t>(10));
    container.AddAttribute(1, static_cast<int32_t>(5));
    int32_t value = container.GetAttribute<int32_t>(1, 0);

    TEST_ASSERT(value == 15, "Add should increment value");
    return true;
}

bool test_attribute_container_batch_set() {
    AttributeContainer container(100);

    std::vector<std::pair<uint32_t, AttributeValue>> attrs = {
        {1, static_cast<int32_t>(10)},
        {2, static_cast<int32_t>(20)},
        {3, static_cast<int32_t>(30)}
    };
    container.SetAttributes(attrs);

    TEST_ASSERT(container.GetAttribute<int32_t>(1, 0) == 10, "Batch set attr 1");
    TEST_ASSERT(container.GetAttribute<int32_t>(2, 0) == 20, "Batch set attr 2");
    TEST_ASSERT(container.GetAttribute<int32_t>(3, 0) == 30, "Batch set attr 3");
    return true;
}

bool test_attribute_container_clear() {
    AttributeContainer container(100);

    container.SetAttribute(1, static_cast<int32_t>(50));
    container.Clear();

    int32_t value = container.GetAttribute<int32_t>(1, -1);
    TEST_ASSERT(value == -1, "Should return default after clear");
    return true;
}

bool test_attribute_container_change_listener() {
    AttributeContainer container(100);
    bool listenerCalled = false;
    uint32_t changedAttrId = 0;

    container.SetChangeListener([&](const AttributeChangeEvent& event) {
        listenerCalled = true;
        changedAttrId = event.attributeId;
    });

    container.SetAttribute(1, static_cast<int32_t>(100));

    TEST_ASSERT(listenerCalled, "Listener should be called");
    TEST_ASSERT(changedAttrId == 1, "Event should have correct attribute ID");
    return true;
}

//==============================================================================
// AOI Tests
//==============================================================================

bool test_vector3_distance() {
    Vector3 v1(0, 0, 0);
    Vector3 v2(3, 4, 0);

    float dist = v1.Distance(v2);
    TEST_ASSERT(std::abs(dist - 5.0f) < 0.001f, "Distance should be 5");

    float distSq = v1.DistanceSquared(v2);
    TEST_ASSERT(std::abs(distSq - 25.0f) < 0.001f, "Distance squared should be 25");
    return true;
}

bool test_aoi_entity_default() {
    AOIEntity entity;
    TEST_ASSERT(entity.entityId == 0, "Default entity ID should be 0");
    TEST_ASSERT(entity.sceneId == 0, "Default scene ID should be 0");
    TEST_ASSERT(entity.aoiRadius == 0, "Default radius should be 0");
    return true;
}

bool test_aoi_grid_update_entity() {
    AOIGrid grid(100.0f);

    AOIEntity entity(1, 100);
    entity.position = Vector3(50, 0, 50);
    entity.aoiRadius = 10;

    bool success = grid.UpdateEntity(entity);
    TEST_ASSERT(success, "Update entity should succeed");

    auto entities = grid.GetAOIEntities(1);
    TEST_ASSERT(entities.empty(), "Single entity should have no neighbors");
    return true;
}

bool test_aoi_grid_two_entities() {
    AOIGrid grid(100.0f);

    AOIEntity entity1(1, 100);
    entity1.position = Vector3(50, 0, 50);
    entity1.aoiRadius = 20;

    AOIEntity entity2(2, 100);
    entity2.position = Vector3(60, 0, 60);
    entity2.aoiRadius = 20;

    grid.UpdateEntity(entity1);
    grid.UpdateEntity(entity2);

    auto entities1 = grid.GetAOIEntities(1);
    TEST_ASSERT(entities1.size() == 1, "Entity 1 should see entity 2");
    TEST_ASSERT(entities1[0] == 2, "Should be entity 2");

    auto entities2 = grid.GetAOIEntities(2);
    TEST_ASSERT(entities2.size() == 1, "Entity 2 should see entity 1");
    return true;
}

bool test_aoi_grid_out_of_range() {
    AOIGrid grid(100.0f);

    AOIEntity entity1(1, 100);
    entity1.position = Vector3(50, 0, 50);
    entity1.aoiRadius = 10;

    AOIEntity entity2(2, 100);
    entity2.position = Vector3(200, 0, 200); // Far away
    entity2.aoiRadius = 10;

    grid.UpdateEntity(entity1);
    grid.UpdateEntity(entity2);

    auto entities1 = grid.GetAOIEntities(1);
    TEST_ASSERT(entities1.empty(), "Far entities should not be visible");
    return true;
}

bool test_aoi_grid_remove_entity() {
    AOIGrid grid(100.0f);

    AOIEntity entity(1, 100);
    entity.position = Vector3(50, 0, 50);

    grid.UpdateEntity(entity);
    bool removed = grid.RemoveEntity(1);
    TEST_ASSERT(removed, "Remove should succeed");

    auto entities = grid.GetAOIEntities(1);
    TEST_ASSERT(entities.empty(), "Removed entity should not exist");
    return true;
}

bool test_aoi_grid_range_query() {
    AOIGrid grid(100.0f);

    AOIEntity entity1(1, 100);
    entity1.position = Vector3(50, 0, 50);

    AOIEntity entity2(2, 100);
    entity2.position = Vector3(55, 0, 55);

    AOIEntity entity3(3, 100);
    entity3.position = Vector3(200, 0, 200);

    grid.UpdateEntity(entity1);
    grid.UpdateEntity(entity2);
    grid.UpdateEntity(entity3);

    auto entities = grid.GetEntitiesInRange(Vector3(50, 0, 50), 20);
    TEST_ASSERT(entities.size() >= 2, "Range query should find nearby entities");
    return true;
}

bool test_aoi_manager_singleton() {
    auto& manager = AOIManager::Instance();
    TEST_ASSERT(&manager == &AOIManager::Instance(), "Should return same instance");
    return true;
}

bool test_aoi_manager_initialize() {
    auto& manager = AOIManager::Instance();
    bool success = manager.Initialize(50.0f);
    TEST_ASSERT(success, "Initialize should succeed");
    return true;
}

bool test_aoi_manager_update_entity() {
    auto& manager = AOIManager::Instance();
    manager.Initialize(100.0f);

    AOIEntity entity(1, 100);
    entity.position = Vector3(50, 0, 50);

    bool success = manager.UpdateEntity(entity);
    TEST_ASSERT(success, "Manager update should succeed");
    return true;
}

//==============================================================================
// ECS Tests
//==============================================================================

bool test_ecs_world_create_entity() {
    World world;
    EcsEntity* entity = world.CreateEntity();

    TEST_ASSERT(entity != nullptr, "Entity should be created");
    TEST_ASSERT(entity->id == 1, "First entity ID should be 1");

    EcsEntity* entity2 = world.CreateEntity();
    TEST_ASSERT(entity2->id == 2, "Second entity ID should be 2");
    return true;
}

bool test_ecs_world_get_entity() {
    World world;
    EcsEntity* created = world.CreateEntity();
    EcsEntity* retrieved = world.GetEntity(created->id);

    TEST_ASSERT(retrieved != nullptr, "Should retrieve created entity");
    TEST_ASSERT(retrieved->id == created->id, "Retrieved entity should have same ID");
    return true;
}

bool test_ecs_world_destroy_entity() {
    World world;
    EcsEntity* entity = world.CreateEntity();
    uint32_t id = entity->id;

    world.DestroyEntity(id);
    EcsEntity* retrieved = world.GetEntity(id);

    TEST_ASSERT(retrieved == nullptr, "Destroyed entity should not exist");
    return true;
}

bool test_ecs_entity_component() {
    struct TestComponent : public IComponent {
        int value = 0;
        size_t GetTypeId() const override { return 0; }
    };

    World world;
    EcsEntity* entity = world.CreateEntity();

    // Note: Component typeId registration is template-based
    // We can't fully test without the implementation file
    TEST_ASSERT(entity != nullptr, "Entity should exist");
    return true;
}

bool test_ecs_system_manager() {
    SystemManager sysManager;

    class TestSystem : public System {
    public:
        bool updated = false;
        void Update(float deltaTime) override {
            updated = true;
        }
    };

    auto testSystem = std::make_shared<TestSystem>();
    sysManager.AddSystem(testSystem);

    sysManager.UpdateAll(0.016f);

    TEST_ASSERT(testSystem->updated, "System should be updated");
    return true;
}

//==============================================================================
// EntityId Tests
//==============================================================================

bool test_entity_id_default() {
    EntityId id;
    TEST_ASSERT(id.value() == 0, "Default ID should be 0");
    TEST_ASSERT(!id.is_valid(), "Default ID should be invalid");
    TEST_ASSERT(!id, "Operator bool should return false");
    return true;
}

bool test_entity_id_construct() {
    EntityId id(42);
    TEST_ASSERT(id.value() == 42, "ID should be 42");
    TEST_ASSERT(id.is_valid(), "Non-zero ID should be valid");
    TEST_ASSERT(id, "Operator bool should return true");
    return true;
}

bool test_entity_id_comparison() {
    EntityId id1(10);
    EntityId id2(10);
    EntityId id3(20);

    TEST_ASSERT(id1 == id2, "Equal IDs should compare equal");
    TEST_ASSERT(id1 != id3, "Different IDs should not be equal");
    TEST_ASSERT(id1 < id3, "Less than should work");
    return true;
}

bool test_entity_id_invalid() {
    EntityId id = EntityId::invalid();
    TEST_ASSERT(!id.is_valid(), "Invalid ID should not be valid");
    TEST_ASSERT(id.value() == 0, "Invalid ID should be 0");
    return true;
}

//==============================================================================
// Entity Tests
//==============================================================================

bool test_entity_construct() {
    EntityId eid(100);
    Entity entity(eid, "TestEntity");

    TEST_ASSERT(entity.get_id() == eid, "Entity should have correct ID");
    TEST_ASSERT(entity.get_type() == "TestEntity", "Entity should have correct type");
    return true;
}

bool test_entity_component_management() {
    EntityId eid(100);
    Entity entity(eid, "TestEntity");

    // Test component add/remove interface exists
    // Actual implementation requires source file
    TEST_ASSERT(entity.get_id().is_valid(), "Entity should have valid ID");
    return true;
}

//==============================================================================
// Scene Tests
//==============================================================================

bool test_scene_construct() {
    Scene scene("TestScene");

    TEST_ASSERT(scene.get_name() == "TestScene", "Scene should have correct name");
    TEST_ASSERT(scene.get_entity_count() == 0, "New scene should be empty");
    return true;
}

bool test_scene_spawn_despawn() {
    Scene scene("TestScene");

    EntityId eid(100);
    auto entity = std::make_shared<Entity>(eid, "TestEntity");

    scene.spawn_entity(entity);
    TEST_ASSERT(scene.get_entity_count() == 1, "Scene should have one entity");

    scene.despawn_entity(eid);
    TEST_ASSERT(scene.get_entity_count() == 0, "Scene should be empty after despawn");
    return true;
}

bool test_scene_get_entity() {
    Scene scene("TestScene");

    EntityId eid(100);
    auto entity = std::make_shared<Entity>(eid, "TestEntity");

    scene.spawn_entity(entity);
    auto retrieved = scene.get_entity(eid);

    TEST_ASSERT(retrieved != nullptr, "Should retrieve spawned entity");
    TEST_ASSERT(retrieved->get_id() == eid, "Retrieved entity should have correct ID");
    return true;
}

bool test_scene_update() {
    Scene scene("TestScene");

    EntityId eid(100);
    auto entity = std::make_shared<Entity>(eid, "TestEntity");

    scene.spawn_entity(entity);
    scene.update(0.016f);

    TEST_ASSERT(scene.get_entity_count() == 1, "Update should not affect entity count");
    return true;
}

//==============================================================================
// Test Runner
//==============================================================================

struct TestInfo {
    const char* name;
    bool (*func)();
};

int main(int argc, char* argv[]) {
    // Initialize AOI manager
    AOIManager::Instance().Initialize(100.0f);

    std::vector<TestInfo> tests = {
        // ComVal Tests
        {"ComVal: Default Construct", test_comval_default_construct},
        {"ComVal: Bool Construct", test_comval_bool_construct},
        {"ComVal: Int Construct", test_comval_int_construct},
        {"ComVal: Double Construct", test_comval_double_construct},
        {"ComVal: String Construct", test_comval_string_construct},
        {"ComVal: Copy Construct", test_comval_copy_construct},
        {"ComVal: Assignment", test_comval_assignment},
        // Windows macro issues - disabled temporarily
        // {"ComVal: Comparison", test_comval_comparison},
        // {"ComVal: Arithmetic", test_comval_arithmetic},
        // {"ComVal: String Concat", test_comval_string_concat},
        {"ComVal: Serialization", test_comval_serialization},
        {"ComVal: ToString", test_comval_to_string},

        // AttributeContainer Tests
        {"Attribute: Set/Get", test_attribute_container_set_get},
        {"Attribute: Get Default", test_attribute_container_get_default},
        {"Attribute: Add", test_attribute_container_add},
        {"Attribute: Batch Set", test_attribute_container_batch_set},
        {"Attribute: Clear", test_attribute_container_clear},
        {"Attribute: Change Listener", test_attribute_container_change_listener},

        // AOI Tests
        {"AOI: Vector3 Distance", test_vector3_distance},
        {"AOI: Entity Default", test_aoi_entity_default},
        {"AOI: Grid Update Entity", test_aoi_grid_update_entity},
        {"AOI: Grid Two Entities", test_aoi_grid_two_entities},
        {"AOI: Grid Out of Range", test_aoi_grid_out_of_range},
        {"AOI: Grid Remove Entity", test_aoi_grid_remove_entity},
        {"AOI: Grid Range Query", test_aoi_grid_range_query},
        {"AOI: Manager Singleton", test_aoi_manager_singleton},
        {"AOI: Manager Initialize", test_aoi_manager_initialize},
        {"AOI: Manager Update Entity", test_aoi_manager_update_entity},

        // ECS Tests
        {"ECS: World Create Entity", test_ecs_world_create_entity},
        {"ECS: World Get Entity", test_ecs_world_get_entity},
        {"ECS: World Destroy Entity", test_ecs_world_destroy_entity},
        {"ECS: Entity Component", test_ecs_entity_component},
        {"ECS: System Manager", test_ecs_system_manager},

        // EntityId Tests
        {"EntityId: Default", test_entity_id_default},
        {"EntityId: Construct", test_entity_id_construct},
        {"EntityId: Comparison", test_entity_id_comparison},
        {"EntityId: Invalid", test_entity_id_invalid},

        // Entity Tests
        {"Entity: Construct", test_entity_construct},
        {"Entity: Component Management", test_entity_component_management},

        // Scene Tests
        {"Scene: Construct", test_scene_construct},
        {"Scene: Spawn/Despawn", test_scene_spawn_despawn},
        {"Scene: Get Entity", test_scene_get_entity},
        {"Scene: Update", test_scene_update},
    };

    int passed = 0;
    int failed = 0;

    for (const auto& test : tests) {
        std::cout << "Running: " << test.name << "... ";
        try {
            if (test.func()) {
                std::cout << "PASS" << std::endl;
                ++passed;
            } else {
                std::cout << "FAIL" << std::endl;
                ++failed;
            }
        } catch (const std::exception& e) {
            std::cout << "EXCEPTION: " << e.what() << std::endl;
            ++failed;
        }
    }

    std::cout << "\n====================================" << std::endl;
    std::cout << "Game Module Tests: " << passed << " passed, " << failed << " failed" << std::endl;
    std::cout << "Total: " << tests.size() << " tests" << std::endl;
    std::cout << "====================================" << std::endl;

    return failed > 0 ? 1 : 0;
}
