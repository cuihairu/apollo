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

// D26 首切片（仅代码更新·样本级验收）：handler 面纯内部改动（全局名/路由面
// 不动）→ 装载闸绿且契约面与基线全等（version + routes 逐条）——「仅代码
// 更新保持编号、字段与 wire」的样本证据；对照第二测：破坏 wire 的更新
// （handler 全局缺失）被装载闸红且 fault 点名，编号/路由漂移不得静默过闸。
TEST(ContractHandlers, CodeOnlyPatchKeepsContractWireFace) {
    const std::string handlers = read_whole_file(APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA);
    const std::string contract = read_whole_file(APOLLO_GENERATED_CONTRACT_LUA);
    ASSERT_FALSE(handlers.empty());
    ASSERT_FALSE(contract.empty());

    apollo::script::ScriptRuntime rt_base(make_config());
    ASSERT_TRUE(rt_base.call_string("contract_handlers", handlers));
    ASSERT_TRUE(rt_base.load_contract(contract));
    const auto base_version = rt_base.contract_version();
    const auto base_routes = rt_base.contract_routes();
    ASSERT_FALSE(base_routes.empty());

    // 仅代码更新样本：首个 handler 内部改写（局部状态+等价无操作），全局名
    // 与签名不动——维护者改 handler 内部而不动路由面的最小形态。
    const std::string needle = "session_up = function() end";
    const std::size_t at = handlers.find(needle);
    ASSERT_NE(at, std::string::npos) << "shipped placeholder line not found";
    const std::string patched = handlers.substr(0, at)
        + "session_up = function() local _kept = true; _kept = not _kept end"
        + handlers.substr(at + needle.size());
    ASSERT_NE(patched, handlers);

    apollo::script::ScriptRuntime rt_new(make_config());
    std::string last_fault;
    rt_new.set_fault_sink([&](const apollo::script::ScriptFaultEvent& e) {
        last_fault = e.message;
    });
    ASSERT_TRUE(rt_new.call_string("contract_handlers", patched))
        << (last_fault.empty() ? "" : last_fault);
    ASSERT_TRUE(rt_new.load_contract(contract))
        << (last_fault.empty() ? "" : last_fault);

    // wire 面全等：契约版本与编号→handler 映射逐条一致（编号/字段未变）。
    EXPECT_EQ(base_version, rt_new.contract_version());
    EXPECT_EQ(base_routes, rt_new.contract_routes());
}

TEST(ContractHandlers, WireBreakingHandlerUpdateRejectedByLoadGate) {
    const std::string handlers = read_whole_file(APOLLO_WORLD_APP_CONTRACT_HANDLERS_LUA);
    const std::string contract = read_whole_file(APOLLO_GENERATED_CONTRACT_LUA);
    ASSERT_FALSE(handlers.empty());
    ASSERT_FALSE(contract.empty());

    // 对照基线：shipped 过闸。
    apollo::script::ScriptRuntime rt_ok(make_config());
    ASSERT_TRUE(rt_ok.call_string("contract_handlers", handlers));
    ASSERT_TRUE(rt_ok.load_contract(contract));

    // 破坏 wire 的更新样本：末行 handler 缺失（route 裸名解析不到）→ 闸红。
    const std::string needle = "space_position_projection = function() end\n";
    const std::size_t at = handlers.find(needle);
    ASSERT_NE(at, std::string::npos) << "shipped placeholder line not found";
    const std::string broken = handlers.substr(0, at) + handlers.substr(at + needle.size());
    ASSERT_NE(broken, handlers);

    apollo::script::ScriptRuntime rt_bad(make_config());
    std::string last_fault;
    rt_bad.set_fault_sink([&](const apollo::script::ScriptFaultEvent& e) {
        last_fault = e.message;
    });
    ASSERT_TRUE(rt_bad.call_string("contract_handlers", broken));
    EXPECT_FALSE(rt_bad.load_contract(contract)) << "wire-breaking update must not load";
    EXPECT_NE(last_fault.find("route handler missing: space_position_projection"),
              std::string::npos)
        << (last_fault.empty() ? "(no fault)" : last_fault);
    // 拒换纪律：闸红后旧数据面不动（首载即红 = 空面，version/routes 保持零值）。
    EXPECT_EQ(rt_bad.contract_version(), 0u);
    EXPECT_TRUE(rt_bad.contract_routes().empty());
}
