#include "apollo/game/attributes/attribute_sync.hpp"
#include <algorithm>
#include <tuple>

namespace apollo {
namespace game {

namespace {

uint32_t audienceKey(AttributeSyncAudience audience) {
    return static_cast<uint32_t>(audience);
}

} // namespace

AttributeSyncAudience operator|(AttributeSyncAudience lhs, AttributeSyncAudience rhs) {
    return static_cast<AttributeSyncAudience>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

AttributeSyncAudience operator&(AttributeSyncAudience lhs, AttributeSyncAudience rhs) {
    return static_cast<AttributeSyncAudience>(static_cast<uint32_t>(lhs) & static_cast<uint32_t>(rhs));
}

AttributeSyncAudience& operator|=(AttributeSyncAudience& lhs, AttributeSyncAudience rhs) {
    lhs = lhs | rhs;
    return lhs;
}

bool hasAudience(AttributeSyncAudience mask, AttributeSyncAudience flag) {
    return static_cast<uint32_t>(mask & flag) != 0;
}

void AttributeSyncSchema::setDefaultRule(AttributeSyncRule rule) {
    defaultRule_ = std::move(rule);
}

void AttributeSyncSchema::registerRule(AttributeSyncRule rule) {
    rules_[rule.attributeId] = std::move(rule);
}

const AttributeSyncRule& AttributeSyncSchema::resolve(uint32_t attributeId) const {
    const auto it = rules_.find(attributeId);
    if (it != rules_.end()) {
        return it->second;
    }
    return defaultRule_;
}

AttributeSyncController::AttributeSyncController(uint64_t objectId, const AttributeSyncSchema* schema)
    : objectId_(objectId), schema_(schema) {
}

AttributeSyncController::AttributeSyncController(const std::shared_ptr<AttributeContainer>& container,
                                                 const AttributeSyncSchema* schema)
    : objectId_(container ? container->objectId() : 0), schema_(schema) {
    bind(container);
}

AttributeSyncController::~AttributeSyncController() {
    unbind();
}

void AttributeSyncController::bind(const std::shared_ptr<AttributeContainer>& container) {
    unbind();

    container_ = container;
    objectId_ = container ? container->objectId() : objectId_;
    if (!container_) {
        return;
    }

    listenerId_ = container_->addChangeListener(
        [this](const AttributeChangeEvent& event) {
            recordChange(event.attributeId, event.newValue, event.fromServer, 0);
        });
}

void AttributeSyncController::unbind() {
    if (container_ && listenerId_ != 0) {
        container_->removeChangeListener(listenerId_);
    }
    listenerId_ = 0;
    container_.reset();
}

uint64_t AttributeSyncController::recordChange(uint32_t attributeId,
                                               const ComVal& value,
                                               bool fromServer,
                                               uint64_t nowMs) {
    if (fromServer) {
        latestEntries_[attributeId] = AttributeSyncEntry{
            attributeId,
            value,
            latestEntries_[attributeId].version,
            nowMs,
            schema()->resolve(attributeId),
        };
        return currentVersion_;
    }

    const AttributeSyncRule rule = schema()->resolve(attributeId);
    if (!rule.allowDelta || rule.audiences == AttributeSyncAudience::None) {
        latestEntries_[attributeId] = AttributeSyncEntry{
            attributeId,
            value,
            ++currentVersion_,
            nowMs,
            rule,
        };
        return currentVersion_;
    }

    const uint64_t version = ++currentVersion_;
    AttributeSyncEntry entry{attributeId, value, version, nowMs, rule};
    latestEntries_[attributeId] = entry;

    auto audiences = splitAudiences(rule.audiences);
    for (const auto audience : audiences) {
        auto& state = channel(audience);
        auto& pending = state.pending[attributeId];
        pending.entry = entry;
        const auto sentIt = state.lastSentAtMsByAttribute.find(attributeId);
        pending.readyAtMs = sentIt == state.lastSentAtMsByAttribute.end()
            ? nowMs
            : std::max(nowMs, sentIt->second + rule.minIntervalMs);
    }

    return version;
}

std::vector<AttributeSyncBatch> AttributeSyncController::collectDeltaBatches(uint64_t nowMs,
                                                                             std::size_t maxEntriesPerBatch) {
    std::vector<AttributeSyncBatch> batches;

    for (auto& [audienceBits, state] : channels_) {
        auto audience = static_cast<AttributeSyncAudience>(audienceBits);

        using GroupKey = std::tuple<AttributeSyncPriority, AttributeSyncReliability>;
        std::unordered_map<uint32_t, std::vector<AttributeSyncEntry>> groups;
        std::vector<uint32_t> groupOrder;
        std::vector<uint32_t> removable;

        for (const auto& [attributeId, pending] : state.pending) {
            if (pending.readyAtMs > nowMs) {
                continue;
            }

            const auto key = (static_cast<uint32_t>(pending.entry.rule.priority) << 8)
                | static_cast<uint32_t>(pending.entry.rule.reliability);
            if (!groups.contains(key)) {
                groupOrder.push_back(key);
            }
            groups[key].push_back(pending.entry);
        }

        for (const auto key : groupOrder) {
            auto& entries = groups[key];
            std::sort(entries.begin(),
                      entries.end(),
                      [](const auto& lhs, const auto& rhs) {
                          return lhs.version < rhs.version;
                      });

            for (std::size_t offset = 0; offset < entries.size(); offset += maxEntriesPerBatch) {
                const auto end = std::min(entries.size(), offset + maxEntriesPerBatch);
                std::vector<AttributeSyncEntry> slice(entries.begin() + static_cast<std::ptrdiff_t>(offset),
                                                      entries.begin() + static_cast<std::ptrdiff_t>(end));
                auto batch = makeBatch(nowMs,
                                       audience,
                                       slice.front().rule.priority,
                                       slice.front().rule.reliability,
                                       std::move(slice));

                for (const auto& entry : batch.entries) {
                    state.lastSentAtMsByAttribute[entry.attributeId] = nowMs;
                    removable.push_back(entry.attributeId);
                }

                if (batch.reliability == AttributeSyncReliability::Reliable) {
                    state.inflight.push_back(batch);
                }
                batches.push_back(std::move(batch));
            }
        }

        for (const auto attributeId : removable) {
            state.pending.erase(attributeId);
        }
    }

    std::sort(batches.begin(),
              batches.end(),
              [](const auto& lhs, const auto& rhs) {
                  if (lhs.audience != rhs.audience) {
                      return static_cast<uint32_t>(lhs.audience) < static_cast<uint32_t>(rhs.audience);
                  }
                  if (lhs.priority != rhs.priority) {
                      return static_cast<uint8_t>(lhs.priority) < static_cast<uint8_t>(rhs.priority);
                  }
                  return lhs.toVersion < rhs.toVersion;
              });
    return batches;
}

std::vector<AttributeSyncBatch> AttributeSyncController::collectRetryBatches(uint64_t nowMs,
                                                                             uint64_t retryAfterMs) const {
    std::vector<AttributeSyncBatch> retries;

    for (const auto& [_, state] : channels_) {
        for (const auto& batch : state.inflight) {
            if (nowMs >= batch.emittedAtMs + retryAfterMs) {
                retries.push_back(batch);
            }
        }
    }

    std::sort(retries.begin(),
              retries.end(),
              [](const auto& lhs, const auto& rhs) {
                  return lhs.toVersion < rhs.toVersion;
              });
    return retries;
}

AttributeSyncBatch AttributeSyncController::buildFullBatch(AttributeSyncAudience audience) const {
    AttributeSyncBatch batch;
    batch.objectId = objectId_;
    batch.audience = audience;
    batch.mode = AttributeSyncMode::Full;
    batch.reliability = AttributeSyncReliability::Reliable;
    batch.priority = AttributeSyncPriority::High;
    batch.toVersion = currentVersion_;

    if (!container_) {
        return batch;
    }

    auto values = container_->getAllValues();
    std::vector<uint32_t> attributeIds;
    attributeIds.reserve(values.size());
    for (const auto& [attributeId, _] : values) {
        attributeIds.push_back(attributeId);
    }
    std::sort(attributeIds.begin(), attributeIds.end());

    for (const auto attributeId : attributeIds) {
        const auto rule = schema()->resolve(attributeId);
        if (!rule.includeInFullSync || !hasAudience(rule.audiences, audience)) {
            continue;
        }

        batch.entries.push_back(AttributeSyncEntry{
            attributeId,
            values[attributeId],
            latestEntries_.contains(attributeId) ? latestEntries_.at(attributeId).version : 0,
            latestEntries_.contains(attributeId) ? latestEntries_.at(attributeId).changedAtMs : 0,
            rule,
        });
    }

    return batch;
}

void AttributeSyncController::acknowledge(AttributeSyncAudience audience, uint64_t version) {
    auto& state = channel(audience);
    state.acknowledgedVersion = std::max(state.acknowledgedVersion, version);
    state.inflight.erase(
        std::remove_if(state.inflight.begin(),
                       state.inflight.end(),
                       [version](const auto& batch) {
                           return batch.toVersion <= version;
                       }),
        state.inflight.end());
}

AttributeSyncBatch AttributeSyncController::makeBatch(uint64_t nowMs,
                                                      AttributeSyncAudience audience,
                                                      AttributeSyncPriority priority,
                                                      AttributeSyncReliability reliability,
                                                      std::vector<AttributeSyncEntry> entries) {
    AttributeSyncBatch batch;
    batch.objectId = objectId_;
    batch.audience = audience;
    batch.mode = AttributeSyncMode::Delta;
    batch.priority = priority;
    batch.reliability = reliability;
    batch.emittedAtMs = nowMs;
    batch.entries = std::move(entries);

    if (!batch.entries.empty()) {
        auto minmaxVersion = std::minmax_element(
            batch.entries.begin(),
            batch.entries.end(),
            [](const auto& lhs, const auto& rhs) {
                return lhs.version < rhs.version;
            });
        batch.fromVersion = minmaxVersion.first->version;
        batch.toVersion = minmaxVersion.second->version;
    }

    return batch;
}

AttributeSyncController::ChannelState& AttributeSyncController::channel(AttributeSyncAudience audience) {
    return channels_[audienceKey(audience)];
}

const AttributeSyncSchema* AttributeSyncController::schema() const {
    static const AttributeSyncSchema kDefaultSchema = [] {
        AttributeSyncSchema schema;
        schema.setDefaultRule(AttributeSyncRule{});
        return schema;
    }();

    return schema_ ? schema_ : &kDefaultSchema;
}

std::vector<AttributeSyncAudience> AttributeSyncController::splitAudiences(AttributeSyncAudience mask) const {
    static constexpr AttributeSyncAudience kAudiences[] = {
        AttributeSyncAudience::Owner,
        AttributeSyncAudience::Aoi,
        AttributeSyncAudience::Team,
        AttributeSyncAudience::Guild,
        AttributeSyncAudience::Service,
        AttributeSyncAudience::Persistence,
    };

    std::vector<AttributeSyncAudience> result;
    for (const auto audience : kAudiences) {
        if (hasAudience(mask, audience)) {
            result.push_back(audience);
        }
    }
    return result;
}

} // namespace game
} // namespace apollo
