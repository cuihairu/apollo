#pragma once

#include "gateway/session_manager.hpp"
#include "apollo/protocol/socket.hpp"

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
    // admit 先收口）；缺省无操作（桩实现无生命周期）。
    virtual void shutdown() {}
};

std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService(
    apollo::protocol::RpcClient* loginAppClient
);

// Link M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面 + linkrpc
// 关联原语。工厂即装配（返回已起面的就绪服务）；null = 装配缺位（路由表
// 缺/Windows），调用方退桩传输。
std::unique_ptr<SessionAdmissionService> makeLinkSessionAdmissionService(
    const std::string& host, std::uint16_t port, const std::string& route_json
);

} // namespace gateway
