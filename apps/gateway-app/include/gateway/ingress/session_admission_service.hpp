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
};

std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService(
    apollo::protocol::RpcClient* loginAppClient
);

} // namespace gateway
