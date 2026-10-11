#include "apollo/net/logpush/log_push_client.hpp"

#include <chrono>

#if !defined(_WIN32)

// POSIX 真实现：内部泵线程单写者——链路收发/重连/补投全归泵拍；push 只
// 互斥入队。发送用 Reliable（跨重连恰一次补投，行序不乱）；链路背压拒收
// = 停排留队（队列才是断连缓冲的权威，link 队列只承接在途）。

#include "apollo/net/link/route_json.hpp"
#include "apollo/net/wire/contract_wire.hpp"
#include "apollo_contract.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

namespace apollo::net::logpush {

namespace {

namespace link = apollo::net::link;
namespace wire = apollo::net::wire;

using apollo::contract::gen::MsgId;

constexpr std::uint16_t id_of(MsgId id) {
    return static_cast<std::uint16_t>(id);
}

// 单调毫秒（泵节拍；与内核注入时钟同口径）
std::uint64_t now_ms() {
    return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                          std::chrono::steady_clock::now().time_since_epoch())
                                          .count());
}

} // namespace

LogPushClient::LogPushClient(LogPushClientConfig config) : config_(std::move(config)) {}

LogPushClient::~LogPushClient() {
    stop();
}

bool LogPushClient::start() {
    if (running_.load(std::memory_order_acquire)) {
        return true;
    }
    route_table_ = std::make_unique<link::RouteTable>();
    if (!config_.route_json.empty()) {
        std::ifstream in(config_.route_json, std::ios::binary);
        std::ostringstream buffer;
        if (in) {
            buffer << in.rdbuf();
        }
#if APOLLO_NET_LINK_HAS_JSON
        if (!link::load_route_table_json(buffer.str(), *route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（坏 JSON = 本地-only 降级，不 throw）
        }
#else
        route_table_.reset();
        return false;
#endif
    } else {
        route_table_.reset();
        return false;
    }

    link::LinkManagerConfig mc;
    mc.self_name = config_.self_name;
    mc.route_version = link::route_version_from_hash(route_table_->schema_hash());
    manager_ = std::make_unique<link::LinkManager>(mc);

    link::EndpointConfig ep;
    ep.name = "logger";  // 对端 collector 的握手身份（dial 目标别名）
    ep.host = config_.host;
    ep.port = config_.port;
    ep.role = link::EndpointRole::kActive;
    if (!manager_->add_endpoint(ep)) {
        manager_.reset();
        route_table_.reset();
        return false;
    }

    running_.store(true, std::memory_order_release);
    pump_thread_ = std::thread([this] { run(); });
    return true;
}

void LogPushClient::stop() {
    if (!running_.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (pump_thread_.joinable()) {
        pump_thread_.join();
    }
    manager_.reset();
    route_table_.reset();
    std::lock_guard<std::mutex> lock(queue_mutex_);
    queue_.clear();
}

bool LogPushClient::push(std::string structured_line) {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (queue_.size() >= config_.queue_cap) {
        ++dropped_;  // 顶格丢新：本地文件恒为真相源，丢推不丢账
        return false;
    }
    queue_.push_back(std::move(structured_line));
    return true;
}

bool LogPushClient::connected() const {
#if !defined(_WIN32)
    auto* link = manager_ ? manager_->link("logger") : nullptr;
    return link != nullptr && link->state() == link::LinkState::kUp;
#else
    return false;
#endif
}

std::uint64_t LogPushClient::pushed() const {
    return pushed_.load(std::memory_order_relaxed);
}

std::uint64_t LogPushClient::dropped() const {
    return dropped_.load(std::memory_order_relaxed);
}

std::size_t LogPushClient::queued() const {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    return queue_.size();
}

void LogPushClient::run() {
    while (running_.load(std::memory_order_acquire)) {
        auto* link = manager_ ? manager_->link("logger") : nullptr;
        if (link != nullptr) {
            // 出队一批（上限一段，防大积压单拍独占泵线程）
            std::deque<std::string> batch;
            {
                std::lock_guard<std::mutex> lock(queue_mutex_);
                if (!queue_.empty()) {
                    const auto take = std::min(queue_.size(), std::size_t{256});
                    batch.assign(std::make_move_iterator(queue_.begin()),
                                 std::make_move_iterator(queue_.begin() +
                                                         static_cast<std::ptrdiff_t>(take)));
                    queue_.erase(queue_.begin(),
                                 queue_.begin() + static_cast<std::ptrdiff_t>(take));
                }
            }
            for (auto& line : batch) {
                std::vector<wire::FieldValue> fields(1);
                fields[0].text = line;
                std::vector<std::uint8_t> payload;
                if (!wire::encode_payload(id_of(MsgId::logger_line_push), fields, payload)) {
                    continue;  // 编码失败不可恢复：落地计数（本地文件仍有）
                }
                if (link->send(id_of(MsgId::logger_line_push), payload.data(),
                               payload.size(), link::SendClass::kReliable) !=
                    link::SendCode::kAccepted) {
                    // 背压/断连：停排留队（队头塞回，下拍再试——行序不乱）
                    std::lock_guard<std::mutex> lock(queue_mutex_);
                    queue_.push_front(std::move(line));
                    break;
                }
                ++pushed_;
            }
        }
        if (manager_) {
            manager_->pump(now_ms());
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.pump_interval_ms));
    }
}

} // namespace apollo::net::logpush

#else // _WIN32

namespace apollo::net::logpush {

LogPushClient::LogPushClient(LogPushClientConfig config) : config_(std::move(config)) {}
LogPushClient::~LogPushClient() = default;

bool LogPushClient::start() {
    // link TCP 底座不进本批（POSIX only）——Windows 退空件：start 假成，
    // push 恒落地（本地文件照写），调用面无 #if。
    return true;
}

void LogPushClient::stop() {}

bool LogPushClient::push(std::string) {
    ++dropped_;
    return false;
}

bool LogPushClient::connected() const { return false; }
std::uint64_t LogPushClient::pushed() const { return 0; }
std::uint64_t LogPushClient::dropped() const { return dropped_.load(std::memory_order_relaxed); }
std::size_t LogPushClient::queued() const { return 0; }

} // namespace apollo::net::logpush

#endif // !_WIN32
