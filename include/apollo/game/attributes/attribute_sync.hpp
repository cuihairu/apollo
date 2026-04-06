#pragma once

#include "apollo/game/attributes/attribute_value.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace apollo {
namespace game {

enum class AttributeSyncAudience : uint32_t {
    None = 0,
    Owner = 1u << 0,
    Aoi = 1u << 1,
    Team = 1u << 2,
    Guild = 1u << 3,
    Service = 1u << 4,
    Persistence = 1u << 5,
    All = Owner | Aoi | Team | Guild | Service | Persistence,
};

enum class AttributeSyncPriority : uint8_t {
    Immediate = 0,
    High = 1,
    Normal = 2,
    Low = 3,
};

enum class AttributeSyncReliability : uint8_t {
    Unreliable = 0,
    Reliable = 1,
};

enum class AttributeSyncMode : uint8_t {
    Delta = 0,
    Full = 1,
};

AttributeSyncAudience operator|(AttributeSyncAudience lhs, AttributeSyncAudience rhs);
AttributeSyncAudience operator&(AttributeSyncAudience lhs, AttributeSyncAudience rhs);
AttributeSyncAudience& operator|=(AttributeSyncAudience& lhs, AttributeSyncAudience rhs);
bool hasAudience(AttributeSyncAudience mask, AttributeSyncAudience flag);

struct AttributeSyncRule {
    uint32_t attributeId = 0;
    AttributeSyncAudience audiences = AttributeSyncAudience::Owner;
    AttributeSyncPriority priority = AttributeSyncPriority::Normal;
    AttributeSyncReliability reliability = AttributeSyncReliability::Reliable;
    uint32_t minIntervalMs = 0;
    bool allowDelta = true;
    bool includeInFullSync = true;
    bool coalesce = true;
};

class AttributeSyncSchema {
public:
    void setDefaultRule(AttributeSyncRule rule);
    void registerRule(AttributeSyncRule rule);
    const AttributeSyncRule& resolve(uint32_t attributeId) const;

private:
    AttributeSyncRule defaultRule_{};
    std::unordered_map<uint32_t, AttributeSyncRule> rules_;
};

struct AttributeSyncEntry {
    uint32_t attributeId = 0;
    ComVal value;
    uint64_t version = 0;
    uint64_t changedAtMs = 0;
    AttributeSyncRule rule{};
};

struct AttributeSyncBatch {
    uint64_t objectId = 0;
    AttributeSyncAudience audience = AttributeSyncAudience::None;
    AttributeSyncMode mode = AttributeSyncMode::Delta;
    AttributeSyncPriority priority = AttributeSyncPriority::Normal;
    AttributeSyncReliability reliability = AttributeSyncReliability::Reliable;
    uint64_t fromVersion = 0;
    uint64_t toVersion = 0;
    uint64_t emittedAtMs = 0;
    std::vector<AttributeSyncEntry> entries;
};

class AttributeSyncController {
public:
    explicit AttributeSyncController(uint64_t objectId, const AttributeSyncSchema* schema = nullptr);
    explicit AttributeSyncController(const std::shared_ptr<AttributeContainer>& container,
                                     const AttributeSyncSchema* schema = nullptr);
    ~AttributeSyncController();

    AttributeSyncController(const AttributeSyncController&) = delete;
    AttributeSyncController& operator=(const AttributeSyncController&) = delete;

    void bind(const std::shared_ptr<AttributeContainer>& container);
    void unbind();

    uint64_t recordChange(uint32_t attributeId,
                          const ComVal& value,
                          bool fromServer,
                          uint64_t nowMs);

    std::vector<AttributeSyncBatch> collectDeltaBatches(uint64_t nowMs,
                                                        std::size_t maxEntriesPerBatch = 32);
    std::vector<AttributeSyncBatch> collectRetryBatches(uint64_t nowMs, uint64_t retryAfterMs) const;
    AttributeSyncBatch buildFullBatch(AttributeSyncAudience audience) const;
    void acknowledge(AttributeSyncAudience audience, uint64_t version);

    uint64_t currentVersion() const noexcept { return currentVersion_; }

private:
    struct PendingEntry {
        AttributeSyncEntry entry;
        uint64_t readyAtMs = 0;
    };

    struct ChannelState {
        uint64_t acknowledgedVersion = 0;
        std::unordered_map<uint32_t, PendingEntry> pending;
        std::unordered_map<uint32_t, uint64_t> lastSentAtMsByAttribute;
        std::vector<AttributeSyncBatch> inflight;
    };

    AttributeSyncBatch makeBatch(uint64_t nowMs,
                                 AttributeSyncAudience audience,
                                 AttributeSyncPriority priority,
                                 AttributeSyncReliability reliability,
                                 std::vector<AttributeSyncEntry> entries);
    ChannelState& channel(AttributeSyncAudience audience);
    const AttributeSyncSchema* schema() const;
    std::vector<AttributeSyncAudience> splitAudiences(AttributeSyncAudience mask) const;

    uint64_t objectId_ = 0;
    const AttributeSyncSchema* schema_ = nullptr;
    std::shared_ptr<AttributeContainer> container_;
    uint64_t listenerId_ = 0;
    uint64_t currentVersion_ = 0;
    std::unordered_map<uint32_t, AttributeSyncEntry> latestEntries_;
    std::unordered_map<uint32_t, ChannelState> channels_;
};

} // namespace game
} // namespace apollo
