#pragma once

#include <cmath>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <cstdint>
#include <mutex>
#include <functional>

namespace apollo {

/// 位置结构
struct Vector3 {
    float x, y, z;

    Vector3(float x = 0, float y = 0, float z = 0) : x(x), y(y), z(z) {}

    float DistanceSquared(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return dx * dx + dy * dy + dz * dz;
    }

    float Distance(const Vector3& other) const {
        return std::sqrt(DistanceSquared(other));
    }
};

/// AOI实体信息
struct AOIEntity {
    uint64_t entityId;
    uint32_t sceneId;
    Vector3 position;
    float aoiRadius;
    uint32_t type;          // 实体类型：玩家、NPC、怪物等
    uint32_t flags;         // 标志位：隐身、死亡等

    AOIEntity(uint64_t id = 0, uint32_t scene = 0)
        : entityId(id), sceneId(scene), position(), aoiRadius(0), type(0), flags(0) {}
};

/// AOI事件类型
enum class AOIEventType {
    ENTER = 1,
    LEAVE = 2,
    SYNC = 3,
    UPDATE = 4
};

/// AOI事件
struct AOIEvent {
    uint64_t entityId;
    uint32_t sceneId;
    AOIEventType type;
    Vector3 position;
    uint64_t timestamp;

    std::unordered_set<uint64_t> enterEntities;  // 进入视野的实体
    std::unordered_set<uint64_t> leaveEntities;  // 离开视野的实体
    std::vector<AOIEntity> syncEntities;         // 同步的实体列表
};

/// AOI监听器
class IAOIListener {
public:
    virtual ~IAOIListener() = default;
    virtual void OnAOIEvent(const AOIEvent& event) = 0;
};

/// 网格单元
class GridCell {
public:
    GridCell(int x, int z) : x_(x), z_(z) {}

    void AddEntity(uint64_t entityId) {
        entities_.insert(entityId);
    }

    void RemoveEntity(uint64_t entityId) {
        entities_.erase(entityId);
    }

    const std::unordered_set<uint64_t>& GetEntities() const {
        return entities_;
    }

    bool IsEmpty() const {
        return entities_.empty();
    }

    int GetX() const { return x_; }
    int GetZ() const { return z_; }

private:
    int x_, z_;
    std::unordered_set<uint64_t> entities_;
};

/// AOI网格实现
class AOIGrid {
public:
    AOIGrid(float cellSize = 100.0f);
    ~AOIGrid() = default;

    /// 更新实体位置
    bool UpdateEntity(const AOIEntity& entity);

    /// 移除实体
    bool RemoveEntity(uint64_t entityId);

    /// 获取实体AOI内的其他实体
    std::vector<uint64_t> GetAOIEntities(uint64_t entityId);

    /// 获取指定范围内的实体
    std::vector<uint64_t> GetEntitiesInRange(const Vector3& center, float radius);

    /// 清空所有实体
    void Clear();

    /// 设置监听器
    void SetListener(std::shared_ptr<IAOIListener> listener) {
        listener_ = listener;
    }

private:
    struct EntityInfo {
        AOIEntity entity;
        int lastGridX, lastGridZ;
    };

    float cellSize_;
    std::unordered_map<uint64_t, EntityInfo> entities_;
    std::unordered_map<uint64_t, std::unique_ptr<GridCell>> gridCells_;
    std::shared_ptr<IAOIListener> listener_;
    mutable std::mutex mutex_;

    GridCell* GetOrCreateCell(int x, int z);
    void RemoveFromGrid(const EntityInfo& info);
    std::vector<GridCell*> GetNearbyCells(int gridX, int gridZ, int radius);
};

/// AOI管理器
class AOIManager {
public:
    static AOIManager& Instance() {
        static AOIManager instance;
        return instance;
    }

    /// 初始化AOI系统
    bool Initialize(float cellSize = 100.0f);

    /// 更新实体
    bool UpdateEntity(const AOIEntity& entity);

    /// 批量更新实体
    void UpdateEntities(const std::vector<AOIEntity>& entities);

    /// 移除实体
    bool RemoveEntity(uint64_t entityId);

    /// 获取实体的可见实体列表
    std::vector<uint64_t> GetVisibleEntities(uint64_t entityId);

    /// 设置监听器
    void SetListener(std::shared_ptr<IAOIListener> listener);

    /// 清理指定场景的所有实体
    void ClearScene(uint32_t sceneId);

    /// 获取统计信息
    struct Stats {
        size_t totalEntities;
        size_t totalGridCells;
        size_t activeGridCells;
    };

    Stats GetStats() const;

private:
    AOIManager() = default;
    ~AOIManager() = default;

    std::unordered_map<uint32_t, std::unique_ptr<AOIGrid>> sceneGrids_;
    std::shared_ptr<IAOIListener> listener_;
    float defaultCellSize_;
    mutable std::mutex mutex_;
};

}  // namespace apollo