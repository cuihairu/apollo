#pragma once

#include "gateway/session_manager.hpp"

#include <memory>
#include <string>

namespace gateway {

struct AdmissionRequest {
    SessionID sessionId{0};
    PlayerID playerId{0};
    std::string loginTicket;
};

struct AdmissionResult {
    bool accepted{false};
    std::string rejectReason;
};

class SessionAdmissionService {
public:
    virtual ~SessionAdmissionService() = default;

    virtual AdmissionResult admit(const AdmissionRequest& request) = 0;

    // 收口面（自带泵线程的实现须先停驱再拆——宿主 stop 序保证 in-flight
    // admit 先收口）；缺省无操作（退化实现无生命周期）。
    virtual void shutdown() {}
};

// 退化容忍面（P4-4 批⑤：nng 桩准入退役后的非 M1 接线面）：仅拒空票，
// 非空票放行——桩时代同缝（桩传输恒 throw → catch 容忍），ingress 主线
// 全接线前保持 admit 容忍（P3-2 收口）。
std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService();

// Link M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面 + linkrpc
// 关联原语。工厂即装配（返回已起面的就绪服务）；null = 装配缺位（路由表
// 缺/Windows），调用方退退化容忍面。
std::unique_ptr<SessionAdmissionService> makeLinkSessionAdmissionService(
    const std::string& host, std::uint16_t port, const std::string& route_json
);

} // namespace gateway
