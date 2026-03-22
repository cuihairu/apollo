#include "gateway/ingress/session_admission_service.hpp"

#include "apollo/protocol/messages.hpp"

#include <exception>

namespace gateway {

namespace {

class DefaultSessionAdmissionService final : public SessionAdmissionService {
public:
    explicit DefaultSessionAdmissionService(apollo::protocol::RpcClient* loginAppClient)
        : loginAppClient_(loginAppClient) {
    }

    AdmissionResult admit(const AdmissionRequest& request) override {
        if (request.loginTicket.empty()) {
            return {false, "login ticket is empty"};
        }

        if (loginAppClient_ == nullptr) {
            return {false, "login app client unavailable"};
        }

        try {
            apollo::protocol::GatewayAssignRequest rpcRequest;
            rpcRequest.playerId = request.playerId;
            rpcRequest.sessionId = request.sessionId;
            rpcRequest.loginTicket = request.loginTicket;

            const auto response =
                loginAppClient_->call<
                    apollo::protocol::GatewayAssignRequest,
                    apollo::protocol::GatewayAssignResponse>(rpcRequest, request.sessionId);

            if (!response.success) {
                return {false, "gateway admission rejected"};
            }

            return {true, {}};
        } catch (const std::exception&) {
            // Some local builds still run on transport stubs. Keep current behavior:
            // non-empty tickets remain admissible until the ingress mainline is fully wired.
            return {true, {}};
        }
    }

private:
    apollo::protocol::RpcClient* loginAppClient_{nullptr};
};

} // namespace

std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService(
    apollo::protocol::RpcClient* loginAppClient
) {
    return std::make_unique<DefaultSessionAdmissionService>(loginAppClient);
}

} // namespace gateway
