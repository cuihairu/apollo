#include "gateway/ingress/client_ingress_server.hpp"

namespace gateway {

namespace {

class NullClientIngressServer final : public ClientIngressServer {
public:
    void start() override {}
    void stop() override {}

    void send(ConnectionID connectionId, std::span<const std::uint8_t> payload) override {
        (void)connectionId;
        (void)payload;
    }

    void close(ConnectionID connectionId, DisconnectReason reason) override {
        (void)connectionId;
        (void)reason;
    }

    void setObserver(IClientIngressObserver* observer) override {
        observer_ = observer;
    }

private:
    IClientIngressObserver* observer_{nullptr};
};

} // namespace

std::unique_ptr<ClientIngressServer> makeNullClientIngressServer() {
    return std::make_unique<NullClientIngressServer>();
}

} // namespace gateway
