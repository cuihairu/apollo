#include "apollo/game/aoi/aoi.hpp"
#include <cmath>

namespace apollo {

GridCell* AOIGrid::GetOrCreateCell(int x, int z) {
    uint64_t key = (static_cast<uint64_t>(x) << 32) | static_cast<uint64_t>(z);
    auto it = gridCells_.find(key);
    if (it != gridCells_.end()) {
        return it->second.get();
    }

    auto cell = std::make_unique<GridCell>(x, z);
    GridCell* ptr = cell.get();
    gridCells_[key] = std::move(cell);
    return ptr;
}  // namespace apollo

void AOIGrid::RemoveFromGrid(const EntityInfo& info) {
    GridCell* cell = GetOrCreateCell(info.lastGridX, info.lastGridZ);
    if (cell) {
        cell->RemoveEntity(info.entity.entityId);
        if (cell->IsEmpty()) {
            uint64_t key = (static_cast<uint64_t>(info.lastGridX) << 32) |
                          static_cast<uint64_t>(info.lastGridZ);
            gridCells_.erase(key);
        }
    }
}

std::vector<GridCell*> AOIGrid::GetNearbyCells(int gridX, int gridZ, int radius) {
    std::vector<GridCell*> cells;
    for (int dx = -radius; dx <= radius; ++dx) {
        for (int dz = -radius; dz <= radius; ++dz) {
            GridCell* cell = GetOrCreateCell(gridX + dx, gridZ + dz);
            cells.push_back(cell);
        }
    }
    return cells;
}

AOIGrid::AOIGrid(float cellSize) : cellSize_(cellSize) {}

bool AOIGrid::UpdateEntity(const AOIEntity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entity.entityId);
    if (it == entities_.end()) {
        // 新实体
        EntityInfo info;
        info.entity = entity;
        info.lastGridX = static_cast<int>(entity.position.x / cellSize_);
        info.lastGridZ = static_cast<int>(entity.position.z / cellSize_);

        entities_[entity.entityId] = info;
        GetOrCreateCell(info.lastGridX, info.lastGridZ)->AddEntity(entity.entityId);

        return true;
    } else {
        // 更新现有实体
        EntityInfo& info = it->second;

        int newGridX = static_cast<int>(entity.position.x / cellSize_);
        int newGridZ = static_cast<int>(entity.position.z / cellSize_);

        if (newGridX != info.lastGridX || newGridZ != info.lastGridZ) {
            // 网格发生变化
            RemoveFromGrid(info);

            info.entity = entity;
            info.lastGridX = newGridX;
            info.lastGridZ = newGridZ;

            GetOrCreateCell(newGridX, newGridZ)->AddEntity(entity.entityId);
        } else {
            // 只更新位置
            info.entity = entity;
        }

        return true;
    }
}

bool AOIGrid::RemoveEntity(uint64_t entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it == entities_.end()) {
        return false;
    }

    RemoveFromGrid(it->second);
    entities_.erase(it);

    if (listener_) {
        AOIEvent event;
        event.entityId = entityId;
        event.sceneId = 0;
        event.type = AOIEventType::LEAVE;
        event.timestamp = 0;
        listener_->OnAOIEvent(event);
    }

    return true;
}

std::vector<uint64_t> AOIGrid::GetAOIEntities(uint64_t entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = entities_.find(entityId);
    if (it == entities_.end()) {
        return {};
    }

    const AOIEntity& entity = it->second.entity;
    int gridX = static_cast<int>(entity.position.x / cellSize_);
    int gridZ = static_cast<int>(entity.position.z / cellSize_);
    int gridRadius = static_cast<int>(entity.aoiRadius / cellSize_) + 1;

    std::vector<uint64_t> result;
    auto cells = GetNearbyCells(gridX, gridZ, gridRadius);

    for (GridCell* cell : cells) {
        for (uint64_t otherId : cell->GetEntities()) {
            if (otherId != entityId) {
                auto otherIt = entities_.find(otherId);
                if (otherIt != entities_.end()) {
                    const AOIEntity& other = otherIt->second.entity;
                    float dist = entity.position.Distance(other.position);
                    float maxDist = entity.aoiRadius + other.aoiRadius;
                    if (dist <= maxDist) {
                        result.push_back(otherId);
                    }
                }
            }
        }
    }

    return result;
}

std::vector<uint64_t> AOIGrid::GetEntitiesInRange(const Vector3& center, float radius) {
    std::lock_guard<std::mutex> lock(mutex_);

    int gridX = static_cast<int>(center.x / cellSize_);
    int gridZ = static_cast<int>(center.z / cellSize_);
    int gridRadius = static_cast<int>(radius / cellSize_) + 1;

    std::vector<uint64_t> result;
    auto cells = GetNearbyCells(gridX, gridZ, gridRadius);

    for (GridCell* cell : cells) {
        for (uint64_t entityId : cell->GetEntities()) {
            auto it = entities_.find(entityId);
            if (it != entities_.end()) {
                const AOIEntity& entity = it->second.entity;
                float dist = center.Distance(entity.position);
                if (dist <= radius) {
                    result.push_back(entityId);
                }
            }
        }
    }

    return result;
}

void AOIGrid::Clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    entities_.clear();
    gridCells_.clear();
}

bool AOIManager::Initialize(float cellSize) {
    defaultCellSize_ = cellSize;
    return true;
}

bool AOIManager::UpdateEntity(const AOIEntity& entity) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto& grid = sceneGrids_[entity.sceneId];
    if (!grid) {
        grid = std::make_unique<AOIGrid>(defaultCellSize_);
    }

    return grid->UpdateEntity(entity);
}

void AOIManager::UpdateEntities(const std::vector<AOIEntity>& entities) {
    for (const auto& entity : entities) {
        UpdateEntity(entity);
    }
}

bool AOIManager::RemoveEntity(uint64_t entityId) {
    std::lock_guard<std::mutex> lock(mutex_);

    // 需要在所有场景中查找该实体
    for (auto& pair : sceneGrids_) {
        if (pair.second->RemoveEntity(entityId)) {
            return true;
        }
    }

    return false;
}

std::vector<uint64_t> AOIManager::GetVisibleEntities(uint64_t entityId) {
    // 需要知道实体属于哪个场景
    std::lock_guard<std::mutex> lock(mutex_);

    for (const auto& pair : sceneGrids_) {
        const auto& entities = pair.second->GetAOIEntities(entityId);
        if (!entities.empty()) {
            return entities;
        }
    }

    return {};
}

void AOIManager::SetListener(std::shared_ptr<IAOIListener> listener) {
    listener_ = listener;

    // 为所有网格设置监听器
    std::lock_guard<std::mutex> lock(mutex_);
    for (auto& pair : sceneGrids_) {
        pair.second->SetListener(listener);
    }
}

void AOIManager::ClearScene(uint32_t sceneId) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = sceneGrids_.find(sceneId);
    if (it != sceneGrids_.end()) {
        it->second->Clear();
    }
}

AOIManager::Stats AOIManager::GetStats() const {
    std::lock_guard<std::mutex> lock(mutex_);

    Stats stats{};
    for (const auto& _ : sceneGrids_) {
        // TODO: 获取网格统计信息
        stats.totalEntities += 0;
        stats.totalGridCells += 0;
        stats.activeGridCells += 0;
    }

    return stats;
}

}  // namespace apollo
