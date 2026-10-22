#include "apollo/net/linkrpc/rpc_channel.hpp"

#include <utility>

namespace apollo::net::linkrpc {

namespace {

namespace wire = apollo::net::wire;

} // namespace

// ---- 服务端受理面 ----

void RpcServer::install(apollo::net::link::InterServerLink& peer, RpcHandler handler) {
    handler_ = std::move(handler);
    peer.dispatcher().set_handler(
        kRpcRequestMsg,
        [this, &peer](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            on_request(peer, data, size);
        });
}

void RpcServer::on_request(apollo::net::link::InterServerLink& peer,
                           const std::uint8_t* data, std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(kRpcRequestMsg, data, size, fields) || fields.size() < 2) {
        ++stats_.decode_failures;
        return;
    }
    const std::int64_t corr = fields[0].integer;
    std::vector<std::uint8_t> body;
    if (handler_) {
        body = handler_(fields[1].bytes.data(), fields[1].bytes.size());
    }
    ++stats_.requests;

    std::vector<wire::FieldValue> out(2);
    out[0].integer = corr;
    out[1].bytes = std::move(body);
    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(kRpcResponseMsg, out, payload)) {
        ++stats_.decode_failures;
        return;
    }
    // 响应不挑链路态：kDown 排队待补投（kReliable 恰一次语义兜底）。
    peer.send(kRpcResponseMsg, payload, apollo::net::link::SendClass::kReliable);
}

// ---- 客户端关联面 ----

RpcClient::RpcClient(apollo::net::link::InterServerLink& peer) : peer_(peer) {
    peer_.dispatcher().set_handler(
        kRpcResponseMsg,
        [this](std::uint16_t, const std::uint8_t* data, std::size_t size) {
            on_response(data, size);
        });
}

RpcClient::~RpcClient() {
    // 摘响应 handler（宿主保证先停泵——best effort 防悬垂）。
    peer_.dispatcher().set_handler(kRpcResponseMsg, nullptr);
}

std::uint64_t RpcClient::call_async(const std::vector<std::uint8_t>& body) {
    if (peer_.state() != apollo::net::link::LinkState::kUp) {
        return 0;  // 断连 = 立即失败（调用方降级），不占表不排队
    }

    std::vector<wire::FieldValue> fields(2);
    std::uint64_t corr = 0;
    {
        std::lock_guard<std::mutex> guard(mutex_);
        corr = next_corr_++;
        pending_.emplace(corr, std::nullopt);
    }
    fields[0].integer = static_cast<std::int64_t>(corr);
    fields[1].bytes = body;

    std::vector<std::uint8_t> payload;
    if (!wire::encode_payload(kRpcRequestMsg, fields, payload) ||
        peer_.send(kRpcRequestMsg, payload, apollo::net::link::SendClass::kReliable) !=
            apollo::net::link::SendCode::kAccepted) {
        std::lock_guard<std::mutex> guard(mutex_);
        pending_.erase(corr);
        return 0;
    }
    return corr;
}

std::optional<std::vector<std::uint8_t>> RpcClient::poll(std::uint64_t corr_id) {
    std::lock_guard<std::mutex> guard(mutex_);
    auto it = pending_.find(corr_id);
    if (it == pending_.end() || !it->second.has_value()) {
        return std::nullopt;
    }
    std::vector<std::uint8_t> body = std::move(*it->second);
    pending_.erase(it);
    return body;
}

std::optional<std::vector<std::uint8_t>> RpcClient::wait(std::uint64_t corr_id,
                                                         std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock<std::mutex> lock(mutex_);
    while (true) {
        auto it = pending_.find(corr_id);
        if (it == pending_.end()) {
            return std::nullopt;  // 已被他处取走/摘除
        }
        if (it->second.has_value()) {
            std::vector<std::uint8_t> body = std::move(*it->second);
            pending_.erase(it);
            return body;
        }
        if (cv_.wait_until(lock, deadline) == std::cv_status::timeout) {
            pending_.erase(corr_id);
            return std::nullopt;
        }
    }
}

std::optional<std::vector<std::uint8_t>> RpcClient::call(
    const std::vector<std::uint8_t>& body, std::chrono::milliseconds timeout) {
    const std::uint64_t corr = call_async(body);
    if (corr == 0) {
        return std::nullopt;
    }
    return wait(corr, timeout);
}

void RpcClient::on_response(const std::uint8_t* data, std::size_t size) {
    std::vector<wire::FieldValue> fields;
    if (!wire::decode_payload(kRpcResponseMsg, data, size, fields) || fields.size() < 2) {
        ++stats_.decode_failures;
        return;
    }
    const auto corr = static_cast<std::uint64_t>(fields[0].integer);
    if (responseObserver_) responseObserver_(corr, fields[1].bytes);
    {
        std::lock_guard<std::mutex> guard(mutex_);
        auto it = pending_.find(corr);
        if (it == pending_.end() || it->second.has_value()) {
            // 无等待者（已超时摘表）或重复响应——迟到弃。
            ++stats_.late_replies;
            return;
        }
        it->second = std::move(fields[1].bytes);
        ++stats_.responses;
    }
    cv_.notify_all();
}

} // namespace apollo::net::linkrpc
