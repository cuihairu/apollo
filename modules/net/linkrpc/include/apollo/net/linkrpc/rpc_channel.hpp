#pragma once

// Link RPC 关联原语（P4-4 批⓪：nng 迁移底座）。
//
// modules/net/rpc 与 protocol::RpcClient/RepSocket 是 nng 旧栈（桩传输，
// 摸底批结论：五处消费面全部死面），本层是 link M1 之上的最小请求/响应
// 关联件：契约 940/941 关联信封 + corr_id 关联 + 同步等待桥。载荷不透明
// ——body = MessageCodec 全帧字节（header+body），业务语义归各进程
// handler（login 准入、manager 控制面等），本层不解释业务字段。
//
// 线程纪律：响应到达与请求受理都在宿主 pump 线程回调——handler 不得
// 阻塞；wait()/call() 阻塞等响应，禁止在 pump 线程调用（自等自达 =
// 死锁）。单线程测试与泵一体化宿主用 call_async + poll 全确定性走。
//
// 可靠语义：请求/响应均 kReliable（断连排队、重连补投、seq 去重）；但
// call_async 仅链路 kUp 时受理（断连 = 立即失败，调用方降级——排队的
// 请求等到重连才上 wire，同步等待方早已超时，纯浪费）；响应发送不挑
// 链路态（kDown 排队待补投）。迟到响应（等待方已超时摘表）= 弃 + 计数。

#include "apollo/net/link/interserver_link.hpp"
#include "apollo/net/wire/contract_wire.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vector>

namespace apollo::net::linkrpc {

// 契约关联信封 id（生成头枚举转值——契约变更自动跟随）。
constexpr std::uint16_t kRpcRequestMsg =
    static_cast<std::uint16_t>(apollo::contract::gen::MsgId::rpc_request);
constexpr std::uint16_t kRpcResponseMsg =
    static_cast<std::uint16_t>(apollo::contract::gen::MsgId::rpc_response);

// 请求受理函数：入参 = 请求 body（信封 Bytes 字段原样），返回 = 响应
// body（原样回带，可空）。在泵线程回调——不得阻塞。
using RpcHandler = std::function<std::vector<std::uint8_t>(const std::uint8_t* body,
                                                           std::size_t size)>;

// 服务端受理面：rpc_request → handler → rpc_response（同 corr_id 回带）。
// install 在 peer_listener 内调用（fan-out 每对端一挂，同 logger serve
// 模式）；本对象须活过 peer 的分发期。
class RpcServer {
public:
    struct Stats {
        std::uint64_t requests = 0;        // 受理并应答数
        std::uint64_t decode_failures = 0; // 信封编解码失败（字段级非法）
    };

    // 挂 rpc_request handler 到 peer 分发器（同 id 重挂 = 覆盖）。
    void install(apollo::net::link::InterServerLink& peer, RpcHandler handler);
    const Stats& stats() const { return stats_; }

private:
    void on_request(apollo::net::link::InterServerLink& peer, const std::uint8_t* data,
                    std::size_t size);

    RpcHandler handler_;
    Stats stats_;  // 泵线程单写者（handler 链内），内核 Stats 同口径
};

// 客户端关联面：corr_id 单调分配（0 保留 = 失败哨）+ pending 表 + 等待桥。
// 本对象须活过 peer 的分发期（析构摘响应 handler； orderly shutdown 由
// 宿主保证——先停泵再析构客户端）。
class RpcClient {
public:
    struct Stats {
        std::uint64_t responses = 0;       // 响应到达数（含迟到）
        std::uint64_t late_replies = 0;    // 迟到（无等待者/重复）弃数
        std::uint64_t decode_failures = 0; // 响应信封解码失败
    };

    explicit RpcClient(apollo::net::link::InterServerLink& peer);
    ~RpcClient();

    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;

    // 非阻塞发起。返回关联号（0 = 失败：链路非 kUp / 背压拒收 / 编码失败）。
    std::uint64_t call_async(const std::vector<std::uint8_t>& body);

    // 非阻塞查收：响应已达 = 取走（摘表）；未达 = nullopt（表项保留）。
    std::optional<std::vector<std::uint8_t>> poll(std::uint64_t corr_id);

    // 阻塞等待（禁 pump 线程）：超时 = 摘表（后续迟到响应弃 + 计数）。
    std::optional<std::vector<std::uint8_t>> wait(std::uint64_t corr_id,
                                                  std::chrono::milliseconds timeout);

    // 便捷封装：发起 + 等待（coroutines 前的最省事同步口）。
    std::optional<std::vector<std::uint8_t>> call(const std::vector<std::uint8_t>& body,
                                                  std::chrono::milliseconds timeout);

    // Pump-thread response observation. Must not block.
    void setResponseObserver(
        std::function<void(std::uint64_t,
                           const std::optional<std::vector<std::uint8_t>>&)> fn) {
        responseObserver_ = std::move(fn);
    }

    const Stats& stats() const { return stats_; }

private:
    void on_response(const std::uint8_t* data, std::size_t size);

    apollo::net::link::InterServerLink& peer_;
    std::mutex mutex_;
    std::condition_variable cv_;
    // corr → 响应体（nullopt = 未达）。std::map 迭代器跨他项增删稳定。
    std::map<std::uint64_t, std::optional<std::vector<std::uint8_t>>> pending_;
    std::uint64_t next_corr_ = 1;  // mutex 内
    Stats stats_;                  // 泵线程单写者（响应链内）
    std::function<void(std::uint64_t,
                       const std::optional<std::vector<std::uint8_t>>&)> responseObserver_;
};

}  // namespace apollo::net::linkrpc
