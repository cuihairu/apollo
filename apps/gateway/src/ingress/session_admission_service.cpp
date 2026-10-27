#include "gateway/ingress/session_admission_service.hpp"

#include "apollo/net/link/link_manager.hpp"
#include "apollo/net/link/route_json.hpp"
#include "apollo/net/link/route_table.hpp"
#include "apollo/net/linkrpc/rpc_channel.hpp"
#include "apollo/protocol/codec.hpp"
#include "apollo/protocol/messages.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <exception>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <thread>
#include <utility>
#include <vector>

namespace gateway {

namespace {

// 退化容忍面（P4-4 批⑤：nng 桩准入退役）：仅拒空票，非空票放行——桩
// 时代同缝（桩传输恒 throw → catch 容忍），ingress 主线全接线前保持
// admit 容忍（P3-2 收口）。
class DefaultSessionAdmissionService final : public SessionAdmissionService {
public:
    AdmissionResult admit(const AdmissionRequest& request) override {
        if (request.loginTicket.empty()) {
            return {false, "login ticket is empty"};
        }

        // Non-empty tickets remain admissible until the ingress mainline is
        // fully wired (P3-2).
        return {true, {}};
    }
};

// Link M1 准入面（P4-4 批①）：active 端点拨 login 准入受理面 + linkrpc
// 关联原语。自带泵线程（LogPushClient 同型）——准入是同步等待语义，泵
// 必须独立于调用方存活（gateway 的准入调用点在 ingress 宿主泵线程上，
// 自泵自等 = 死锁）。
//
// 降级口径与桩时代同缝：传输不可达/信封残缺不放大成拒绝（准入面缺席
// ≠ 会话非法，ingress 主线全接线前保持 admit 容忍）；仅 login 明确
// success=false 才拒。
class LinkSessionAdmissionService final : public SessionAdmissionService {
public:
    LinkSessionAdmissionService(std::string host, std::uint16_t port, std::string route_json)
        : host_(std::move(host))
        , port_(port)
        , route_json_(std::move(route_json)) {}

    ~LinkSessionAdmissionService() override {
        shutdown();
    }

    bool start() {
#if !defined(_WIN32)
        if (running_.load(std::memory_order_acquire)) {
            return true;
        }
        route_table_ = std::make_unique<apollo::net::link::RouteTable>();
        if (!load_route_table(*route_table_)) {
            route_table_.reset();
            return false;  // 装配错误显形（空路径/坏 JSON = 拒起，调用方退桩）
        }

        apollo::net::link::LinkManagerConfig mc;
        mc.self_name = "gateway";
        mc.route_version =
            apollo::net::link::route_version_from_hash(route_table_->schema_hash());
        mc.compat_route_versions = apollo::net::link::compat_route_versions_from_env();
        manager_ = std::make_unique<apollo::net::link::LinkManager>(mc);

        apollo::net::link::EndpointConfig ep;
        ep.name = "login";  // 对端 G-1 组件名（握手身份 = 其 Link self_name）
        ep.host = host_;
        ep.port = port_;
        ep.role = apollo::net::link::EndpointRole::kActive;
        if (!manager_->add_endpoint(ep)) {
            manager_.reset();
            route_table_.reset();
            return false;
        }

        // 链路对象随 add_endpoint 即存在且跨重连稳定——路由表/白名单/
        // 响应 handler 装配一次，重连不复装（rpc_channel.hpp 纪律）。
        auto* peer = manager_->link(ep.name);
        peer->dispatcher().set_route_table(route_table_.get());
        peer->dispatcher().allow_domain("internal");  // RPC 信封族入闸
        client_ = std::make_unique<apollo::net::linkrpc::RpcClient>(*peer);

        running_.store(true, std::memory_order_release);
        pump_thread_ = std::thread([this]() {
            while (running_.load(std::memory_order_acquire)) {
                manager_->pump(now_ms());
                pump_admits();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
        return true;
#else
        return false;  // link TCP 底座 POSIX only——Windows 退桩传输
#endif
    }

    void shutdown() override {
#if !defined(_WIN32)
        if (!running_.exchange(false, std::memory_order_acq_rel)) {
            return;
        }
        if (pump_thread_.joinable()) {
            pump_thread_.join();
        }
        fail_pending_admits();  // 迟到的在途 admit 立即败（不拖到调用方超时）
        // 先停泵再拆面：RpcClient 摘响应 handler 后链路随之析构（in-flight
        // admit 由调用方线程先于 shutdown 收口——宿主 stop 序保证）。
        client_.reset();
        manager_.reset();
        route_table_.reset();
#endif
    }

    AdmissionResult admit(const AdmissionRequest& request) override {
        if (request.loginTicket.empty()) {
            return {false, "login ticket is empty"};
        }
        if (client_ == nullptr) {
            return {false, "login service unavailable"};
        }

        apollo::protocol::GatewayAssignRequest rpc_request;
        rpc_request.playerId = request.playerId;
        rpc_request.sessionId = request.sessionId;
        rpc_request.loginTicket = request.loginTicket;
        auto frame =
            apollo::protocol::MessageCodec::encode(rpc_request, request.sessionId);

        // 链路核心单线程（零内部锁）：请求受理/响应到达都在本类泵线程，
        // 调用方线程直发（call_async → InterServerLink::send）与泵拍并发
        // 是无同步 UB（macOS Debug CI SEGFAULT 根因）——请求过桥到泵线程
        // 受理，调用方同步等完成（logpush 泵内单写者同纪律）。
        auto slot = std::make_shared<AdmitSlot>();
        slot->frame = std::move(frame);
        slot->deadline = std::chrono::steady_clock::now() +
                         std::chrono::milliseconds(kTimeoutMs);
        {
            std::lock_guard<std::mutex> guard(admit_mutex_);
            admit_slots_.push_back(slot);
        }
        std::unique_lock<std::mutex> lock(slot->mutex);
        slot->cv.wait_until(lock, slot->deadline,
                            [&slot] { return slot->done; });
        lock.unlock();
        if (!slot->done || !slot->response) {
            // 无权威确认不准入（超时/停机/受理失败同口径）。
            return {false, "login service unavailable"};
        }
        const auto& response = *slot->response;

        if (response.size() < sizeof(apollo::protocol::MessageHeader)) {
            return {false, "invalid login response"};
        }
        const auto header = apollo::protocol::MessageCodec::parseHeader(response);
        if (static_cast<apollo::protocol::MessageType>(header.type) !=
            apollo::protocol::MessageType::GATEWAY_ASSIGN_RESPONSE) {
            return {false, "invalid login response"};
        }
        try {
            apollo::protocol::GatewayAssignResponse parsed;
            const std::vector<std::uint8_t> body(response.begin() + sizeof(apollo::protocol::MessageHeader),
                                                 response.end());
            apollo::protocol::MessageCodec::decodeBody(body, parsed);
            if (!parsed.success) {
                return {false, "gateway admission rejected"};
            }
            return {true, {}};
        } catch (const std::exception&) {
            return {false, "invalid login response"};
        }
    }

private:
    // 调用方 ↔ 泵线程 admit 过桥槽：泵线程受理（call_async/poll 全在泵拍，
    // rpc_channel 单线程测试口径同款），完成置 done 唤醒等待方。
    struct AdmitSlot {
        std::vector<std::uint8_t> frame;   // MessageCodec 全帧请求
        std::chrono::steady_clock::time_point deadline;
        std::uint64_t corr = 0;            // 泵线程单写者（0 = 未受理）
        std::mutex mutex;
        std::condition_variable cv;
        bool done = false;                 // mutex 内
        std::optional<std::vector<std::uint8_t>> response;  // nullopt = 失败/超时
    };

    static constexpr std::uint32_t kTimeoutMs = 500;  // 准入同步等待上限

    // 泵线程拍内受理：新槽发 round（call_async），在途槽收 round（poll），
    // 到点即摘表完成（迟到响应由 RpcClient 迟到弃数兜底）。
    void pump_admits() {
        if (client_ == nullptr) {
            return;
        }
        std::vector<std::shared_ptr<AdmitSlot>> batch;
        {
            std::lock_guard<std::mutex> guard(admit_mutex_);
            batch.swap(admit_slots_);
        }
        const auto now = std::chrono::steady_clock::now();
        std::vector<std::shared_ptr<AdmitSlot>> requeue;
        for (auto& slot : batch) {
            if (slot->corr == 0) {
                slot->corr = client_->call_async(slot->frame);
            }
            if (slot->corr != 0) {
                if (auto response = client_->poll(slot->corr)) {
                    complete_admit(*slot, std::move(response));
                    continue;
                }
            }
            if (now >= slot->deadline) {
                if (slot->corr != 0) {
                    (void)client_->poll(slot->corr);  // 摘表（迟到响应弃）
                }
                complete_admit(*slot, std::nullopt);
            } else {
                requeue.push_back(std::move(slot));
            }
        }
        if (!requeue.empty()) {
            std::lock_guard<std::mutex> guard(admit_mutex_);
            admit_slots_.insert(admit_slots_.end(),
                                std::make_move_iterator(requeue.begin()),
                                std::make_move_iterator(requeue.end()));
        }
    }

    static void complete_admit(AdmitSlot& slot,
                               std::optional<std::vector<std::uint8_t>> response) {
        std::lock_guard<std::mutex> guard(slot.mutex);
        slot.response = std::move(response);
        slot.done = true;
        slot.cv.notify_all();
    }

    // 停机拍：泵已停，未受理的在途 admit 立即败（等待方不必耗满超时）。
    void fail_pending_admits() {
        std::vector<std::shared_ptr<AdmitSlot>> dropped;
        {
            std::lock_guard<std::mutex> guard(admit_mutex_);
            dropped.swap(admit_slots_);
        }
        for (auto& slot : dropped) {
            complete_admit(*slot, std::nullopt);
        }
    }

    bool load_route_table(apollo::net::link::RouteTable& table) {
        if (route_json_.empty()) {
            return false;
        }
        std::ifstream in(route_json_, std::ios::binary);
        if (!in) {
            return false;
        }
        std::ostringstream buffer;
        buffer << in.rdbuf();
#if APOLLO_NET_LINK_HAS_JSON
        return apollo::net::link::load_route_table_json(buffer.str(), table);
#else
        (void)table;
        return false;
#endif
    }

    static std::uint64_t now_ms() {
        return static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now().time_since_epoch())
                .count());
    }

    std::string host_;
    std::uint16_t port_ = 0;
    std::string route_json_;
#if !defined(_WIN32)
    // link 传输底座 POSIX only（Windows 工厂 start false 退桩，成员同收守卫）
    std::unique_ptr<apollo::net::link::RouteTable> route_table_;
    std::unique_ptr<apollo::net::link::LinkManager> manager_;
    std::thread pump_thread_;
#endif
    std::unique_ptr<apollo::net::linkrpc::RpcClient> client_;
    std::atomic<bool> running_{false};
    std::mutex admit_mutex_;  // admit_slots_ 互斥（调用方 ↔ 泵线程过桥）
    std::vector<std::shared_ptr<AdmitSlot>> admit_slots_;
};

} // namespace

std::unique_ptr<SessionAdmissionService> makeDefaultSessionAdmissionService() {
    return std::make_unique<DefaultSessionAdmissionService>();
}

std::unique_ptr<SessionAdmissionService> makeLinkSessionAdmissionService(
    const std::string& host, std::uint16_t port, const std::string& route_json
) {
    // 工厂即装配：start false = 装配缺位（路由表缺/Windows），返回 null
    // 由调用方退桩传输（起进程不拦，处方①同口径）。
    auto service = std::make_unique<LinkSessionAdmissionService>(host, port, route_json);
    if (!service->start()) {
        return nullptr;
    }
    return service;
}

} // namespace gateway
