#pragma once

// Login M1 服务注册受理面（L123 服务目录首切片）：Base/World 以裸 964
// service_register_request 拨入（同 960 cell_register 惯例——控制面事实登记
// 非业务 RPC），受理点同步裁决（ServiceRegistry）并按 peer 自报名回 965
// service_register_reply。
//
// 与 m1_admission_server 分面：准入是 gateway→login 的 linkrpc 业务调用；
// 服务注册是各服务进程的控制面登记（延迟重拨/换代语义随注册纪律，不走
// RPC 请求响应生命周期）。机制面自带泵线程（10ms 拍，同 admission）；
// registry 由宿主持有（LoginServer），本面只受理不拥有。
//
// m1ServiceRegisterPort = 0 = 面关（缺省形态不接线）；路由表缺位/端口占绑
// = start false（装配错误显形，宿主降级警告）。

#include "login/service_registry.hpp"

#include <cstdint>
#include <memory>
#include <string>

namespace login {

struct M1ServiceRegisterConfig {
    std::string host = "0.0.0.0";
    std::uint16_t port = 0;  // 0 = 内核分配（测试经 listenPort 读回）
    std::string route_json;  // 契约路由表（缺位 = start false）
};

class M1ServiceRegisterServer {
public:
    virtual ~M1ServiceRegisterServer() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;

    // 实占监听端口（面关/未启动 = 0）
    virtual std::uint16_t listenPort() const = 0;

    // 残帧拒收计数（decode 失败/字段数不符；测试/运维观测面）
    virtual std::uint64_t rejectedFrames() const = 0;
};

// 工厂：POSIX 真实现（link passive 端点 + 964 裸帧 dispatch → registry）/
// Windows 退 null 桩（start false = 面关，宿主装配面无 #if）。
std::unique_ptr<M1ServiceRegisterServer> makeM1ServiceRegisterServer(
    M1ServiceRegisterConfig config, ServiceRegistry* registry);

} // namespace login
