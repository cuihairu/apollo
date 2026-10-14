#include "apollo/net/gameconnection/gameconnection.hpp"

namespace apollo::net::gameconnection {

GameConnection::GameConnection(apollo::net::link::LinkConfig config)
    : link_(std::move(config)) {}

void GameConnection::attach(std::unique_ptr<apollo::net::link::ILinkTransport> transport,
                            std::uint64_t now_ms) {
    link_.attach(std::move(transport), now_ms);
}

bool GameConnection::pump(std::uint64_t now_ms) {
    return link_.pump(now_ms);
}

SendCode GameConnection::send(std::uint16_t msg_id, const std::vector<std::uint8_t>& payload,
                              Channel ch, Priority /*p*/) {
    auto cls = (ch == Channel::kReliable) ? apollo::net::link::SendClass::kReliable
                                          : apollo::net::link::SendClass::kDroppable;
    auto rc = link_.send(msg_id, payload, cls);
    return (rc == apollo::net::link::SendCode::kAccepted) ? SendCode::kAccepted
                                                          : SendCode::kBackPressured;
}

SendCode GameConnection::send(std::uint16_t msg_id, const std::uint8_t* payload,
                              std::size_t size, Channel ch, Priority p) {
    std::vector<std::uint8_t> body(payload, payload + size);
    return send(msg_id, body, ch, p);
}

void GameConnection::subscribe(std::uint16_t msg_id,
                               apollo::net::link::Dispatcher::Handler handler) {
    link_.dispatcher().set_handler(msg_id, std::move(handler));
}

void GameConnection::set_route_table(const apollo::net::link::RouteTable* table) {
    link_.dispatcher().set_route_table(table);
}

void GameConnection::allow_domain(std::string domain) {
    link_.dispatcher().allow_domain(std::move(domain));
}

SessionState GameConnection::state() const {
    switch (link_.state()) {
    case apollo::net::link::LinkState::kHandshaking:
        return SessionState::kHandshaking;
    case apollo::net::link::LinkState::kUp:
        return SessionState::kActive;
    case apollo::net::link::LinkState::kIdle:
    case apollo::net::link::LinkState::kDown:
    default:
        return SessionState::kClosed;
    }
}

void GameConnection::close(const std::string& /*reason*/) {
    link_.close();
}

}  // namespace apollo::net::gameconnection
