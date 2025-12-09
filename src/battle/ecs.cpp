#include "apollo/battle/ecs.hpp"

namespace apollo::battle::ecs {

// 静态成员初始化
size_t ComponentManager::nextComponentId_ = 0;

// World 实现
World::World() {
    // 初始化世界
}

World::~World() {
    // 清理所有实体
    entities_.clear();
    // 系统会自动清理
}

}  // namespace apollo::battle::ecs