// §3.4 宿主启动装载·handler 面同步闸（玩法 Lua 批第一片）：
// shipped handler 占位模块（apps/world/scripts/contract_handlers.lua）必须
// 满足 golden 契约（sdks/cpp/generated/contract.lua）的路由完整性闸——
// apollo_gen 新增 reflect 消息而 handler 面没跟上时，此处即刻红。
// 装载闸语义见 ScriptRuntime::load_contract（routes 逐条 resolve_handler）。
#include "apollo/script/script_runtime.hpp"

#include <fstream>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

namespace {

apollo::script::ScriptConfig make_config() {
    apollo::script::ScriptConfig cfg;
    cfg.instruction_budget = 100000;
    cfg.memory_limit_bytes = 1 << 20;
    return cfg;
}

std::string read_whole_file(const char* path) {
    std::ifstream in{path, std::ios::binary};
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}  // namespace

// 随仓 golden（绝对路径四树同源——同 script_tests 的生成器产物纪律）。
#ifndef APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA
#error "APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA not defined"
#endif
#ifndef APOLLO_GENERATED_CONTRACT_LUA
#error "APOLLO_GENERATED_CONTRACT_LUA not defined"
#endif

TEST(ContractHandlers, ShippedHandlersSatisfyGeneratedContract) {
    const std::string handlers = read_whole_file(APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA);
    ASSERT_FALSE(handlers.empty()) << "shipped handler module missing";
    const std::string contract = read_whole_file(APOLLO_GENERATED_CONTRACT_LUA);
    ASSERT_FALSE(contract.empty()) << "golden contract.lua missing";

    apollo::script::ScriptRuntime rt(make_config());
    std::string last_fault;
    rt.set_fault_sink([&](const apollo::script::ScriptFaultEvent& e) {
        last_fault = e.message;  // 闸拒载时点名缺失 handler（诊断面）
    });
    ASSERT_TRUE(rt.call_string("contract_handlers", handlers))
        << (last_fault.empty() ? "" : last_fault);
    EXPECT_TRUE(rt.load_contract(contract))
        << (last_fault.empty() ? "" : last_fault);
}

TEST(ContractHandlers, ContractDataFaceExposedAfterBootLoad) {
    const std::string handlers = read_whole_file(APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA);
    const std::string contract = read_whole_file(APOLLO_GENERATED_CONTRACT_LUA);
    ASSERT_FALSE(handlers.empty());
    ASSERT_FALSE(contract.empty());

    apollo::script::ScriptRuntime rt(make_config());
    ASSERT_TRUE(rt.call_string("contract_handlers", handlers));
    ASSERT_TRUE(rt.load_contract(contract));
    // 数据面：version 数 + routes 非空（§3.4 消费面入口）。
    EXPECT_GT(rt.contract_version(), 0u);
    EXPECT_FALSE(rt.contract_routes().empty());
}
