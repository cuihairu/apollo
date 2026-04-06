#include "apollo/game/attributes/attribute_sync.hpp"
#include <iostream>
#include <memory>

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cout << "  FAIL: " << msg << " (line " << __LINE__ << ")" << std::endl; \
            return false; \
        } \
    } while (0)

using apollo::game::AttributeContainer;
using apollo::game::AttributeSyncAudience;
using apollo::game::AttributeSyncController;
using apollo::game::AttributeSyncPriority;
using apollo::game::AttributeSyncReliability;
using apollo::game::AttributeSyncRule;
using apollo::game::AttributeSyncSchema;
using apollo::game::ComVal;

bool test_sync_fanout_and_grouping() {
    auto container = std::make_shared<AttributeContainer>(1001);

    AttributeSyncSchema schema;
    schema.setDefaultRule(AttributeSyncRule{});
    schema.registerRule(AttributeSyncRule{
        1,
        AttributeSyncAudience::Owner | AttributeSyncAudience::Aoi,
        AttributeSyncPriority::Immediate,
        AttributeSyncReliability::Reliable,
        0,
        true,
        true,
        true,
    });
    schema.registerRule(AttributeSyncRule{
        2,
        AttributeSyncAudience::Owner,
        AttributeSyncPriority::Normal,
        AttributeSyncReliability::Unreliable,
        0,
        true,
        true,
        true,
    });

    AttributeSyncController controller(container, &schema);

    container->setAttribute(1, ComVal(10), false);
    container->setAttribute(2, ComVal(20), false);

    auto batches = controller.collectDeltaBatches(10);
    TEST_ASSERT(batches.size() == 3, "owner immediate + owner normal + aoi immediate");
    TEST_ASSERT(batches[0].audience == AttributeSyncAudience::Owner, "owner batch first");
    TEST_ASSERT(batches[0].entries.size() == 1, "single attr in first batch");
    TEST_ASSERT(batches[0].entries[0].attributeId == 1, "attr 1 in owner reliable batch");
    TEST_ASSERT(batches[1].audience == AttributeSyncAudience::Owner, "owner normal batch second");
    TEST_ASSERT(batches[1].entries[0].attributeId == 2, "attr 2 in owner normal batch");
    TEST_ASSERT(batches[2].audience == AttributeSyncAudience::Aoi, "aoi batch third");
    TEST_ASSERT(batches[2].entries[0].attributeId == 1, "attr 1 reaches aoi");
    return true;
}

bool test_server_changes_do_not_enqueue_delta() {
    auto container = std::make_shared<AttributeContainer>(1002);
    AttributeSyncController controller(container);

    container->setAttribute(1, ComVal(100), true);
    auto batches = controller.collectDeltaBatches(5);
    TEST_ASSERT(batches.empty(), "server-originated updates should not echo back");
    return true;
}

bool test_min_interval_coalesces_updates() {
    auto container = std::make_shared<AttributeContainer>(1003);

    AttributeSyncSchema schema;
    schema.setDefaultRule(AttributeSyncRule{});
    schema.registerRule(AttributeSyncRule{
        1,
        AttributeSyncAudience::Owner,
        AttributeSyncPriority::Normal,
        AttributeSyncReliability::Reliable,
        100,
        true,
        true,
        true,
    });

    AttributeSyncController controller(container, &schema);

    container->setAttribute(1, ComVal(10), false);
    auto first = controller.collectDeltaBatches(0);
    TEST_ASSERT(first.size() == 1, "first change should send immediately");
    TEST_ASSERT(first[0].entries[0].value.getInt() == 10, "first value sent");

    container->setAttribute(1, ComVal(11), false);
    container->setAttribute(1, ComVal(12), false);

    auto early = controller.collectDeltaBatches(50);
    TEST_ASSERT(early.empty(), "second window still cooling down");

    auto late = controller.collectDeltaBatches(100);
    TEST_ASSERT(late.size() == 1, "latest coalesced value should flush once");
    TEST_ASSERT(late[0].entries[0].value.getInt() == 12, "latest value retained");
    return true;
}

bool test_reliable_retry_and_ack() {
    auto container = std::make_shared<AttributeContainer>(1004);
    AttributeSyncController controller(container);

    container->setAttribute(1, ComVal(7), false);
    auto batches = controller.collectDeltaBatches(10);
    TEST_ASSERT(batches.size() == 1, "reliable batch emitted");

    auto retries = controller.collectRetryBatches(50, 100);
    TEST_ASSERT(retries.empty(), "retry timeout not reached");

    retries = controller.collectRetryBatches(120, 100);
    TEST_ASSERT(retries.size() == 1, "retry timeout reached");
    TEST_ASSERT(retries[0].toVersion == batches[0].toVersion, "same batch retried");

    controller.acknowledge(AttributeSyncAudience::Owner, batches[0].toVersion);
    retries = controller.collectRetryBatches(500, 100);
    TEST_ASSERT(retries.empty(), "ack should clear inflight batch");
    return true;
}

bool test_full_batch_uses_schema_filter() {
    auto container = std::make_shared<AttributeContainer>(1005);
    container->setAttribute(1, ComVal(10), false);
    container->setAttribute(2, ComVal(20), false);

    AttributeSyncSchema schema;
    schema.setDefaultRule(AttributeSyncRule{});
    schema.registerRule(AttributeSyncRule{
        1,
        AttributeSyncAudience::Owner,
        AttributeSyncPriority::Normal,
        AttributeSyncReliability::Reliable,
        0,
        true,
        true,
        true,
    });
    schema.registerRule(AttributeSyncRule{
        2,
        AttributeSyncAudience::Service,
        AttributeSyncPriority::Normal,
        AttributeSyncReliability::Reliable,
        0,
        true,
        false,
        true,
    });

    AttributeSyncController controller(container, &schema);
    auto full = controller.buildFullBatch(AttributeSyncAudience::Owner);

    TEST_ASSERT(full.entries.size() == 1, "owner full sync should only include owner-visible attrs");
    TEST_ASSERT(full.entries[0].attributeId == 1, "attr 1 included");
    return true;
}

int main() {
    struct TestCase {
        const char* name;
        bool (*fn)();
    };

    const TestCase tests[] = {
        {"sync_fanout_and_grouping", test_sync_fanout_and_grouping},
        {"server_changes_do_not_enqueue_delta", test_server_changes_do_not_enqueue_delta},
        {"min_interval_coalesces_updates", test_min_interval_coalesces_updates},
        {"reliable_retry_and_ack", test_reliable_retry_and_ack},
        {"full_batch_uses_schema_filter", test_full_batch_uses_schema_filter},
    };

    std::cout << "Running Attribute Sync Tests..." << std::endl;
    int passed = 0;

    for (const auto& test : tests) {
        std::cout << "Test: " << test.name << "... ";
        if (test.fn()) {
            std::cout << "PASS" << std::endl;
            ++passed;
        } else {
            std::cout << "FAIL" << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "Passed: " << passed << "/" << (sizeof(tests) / sizeof(tests[0])) << std::endl;

    return passed == static_cast<int>(sizeof(tests) / sizeof(tests[0])) ? 0 : 1;
}
