#pragma once

// 平台回调受理链（todo L62）：interfaces 入口收到的平台账号/支付回调，
// 解析完即快照入链，业务执行（Lua/渠道验签发货逻辑）在自己的单写者上
// drain——入口泵只做入队 + 回执（202 受理 / 429 顶格拒收），不执行业务。
// interfaces 本就是独立进程，业务耗时既不阻塞入口泵，更不碰游戏模拟。
//
// 口径与 world AcceptanceQueue 同源（ADR-016 有界两级水位）：cap 顶格
// 拒收、告警线越线报一次回落再报（滞回）、拒绝由调用方显式回执（429），
// 不丢旧不静默。业务执行侧（单写者）另加两条预算：单拍消费上限（慢业
// 务不霸占写者一整拍）与滞留过期（迟到的回调不执行、丢弃并计数——渠道
// 回调的重试语义由对端补，本地不追旧账）。
//
// 单写者纪律：drain() 只应由一个业务线程调用（入口泵线程入队、业务线
// 程消费的两线程模型；或宿主单线程先 pump 后 drain）。队列状态有锁保护
// 不会坏，但执行顺序与计数语义以单 drain 调用方为前提。Lua/业务 executor
// 的真实接线 = P3 Base 玩法意图词汇落地后的挂账面（本链交付的是调度面）。

#include "interfaces/http_ingress.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace interfaces {

// 回调快照：验签输入的完整保真面。body = 原始请求体逐字节（未规范化）；
// raw_request = 逐字节原始请求帧（请求行 + 头 + 体，头名保留原始大小写）。
struct CallbackRequest {
    std::uint64_t id{0};  // 受理序号（drain 侧 FIFO 观测/幂等测试面）
    std::string method;
    std::string path;
    std::vector<std::pair<std::string, std::string>> headers;  // 名已小写
    std::vector<std::uint8_t> body;
    std::vector<std::uint8_t> raw_request;
    std::int64_t received_ms{0};  // 受理时刻（steady_clock 毫秒，观测面）
};

// 由入口请求构造快照（main 与测试共用同一构造点，保真口径不漂移）。
inline CallbackRequest snapshot(const HttpRequest& request) {
    CallbackRequest out;
    out.method = request.method;
    out.path = request.path;
    out.headers = request.headers;
    out.body = request.body;
    out.raw_request = request.raw_request;
    out.received_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch())
            .count();
    return out;
}

struct CallbackChainConfig {
    std::size_t cap{1024};            // 顶格拒收线（同 world AcceptanceQueue 起步值）
    std::size_t high_watermark{768};  // 75% 告警线（越线报一次、回落再报）
    std::size_t max_per_pump{64};     // 单拍消费上限（业务执行预算）
    std::chrono::milliseconds expire_after{0};  // 0 = 不过期；>0 = 滞留超期即丢
};

class CallbackChain {
public:
    explicit CallbackChain(CallbackChainConfig config = {})
        : cap_(config.cap)
        , high_watermark_(config.high_watermark > config.cap ? config.cap
                                                             : config.high_watermark)
        , max_per_pump_(config.max_per_pump == 0 ? 1 : config.max_per_pump)
        , expire_after_(config.expire_after) {}

    struct PushResult {
        bool accepted{false};
        bool high_watermark{false};  // 本次入队后越告警线（仅 accepted 时有意义）
        std::uint64_t id{0};         // 受理分配的受理号（拒收 = 0）
    };

    // 入口泵侧：入队（顶格拒收 = false，调用方回 429，不丢旧不静默）。
    PushResult push(CallbackRequest request) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (queue_.size() >= cap_) {
            ++rejected_;
            return {false, false, 0};
        }
        Entry entry;
        entry.request = std::move(request);
        entry.request.id = next_id_++;
        const std::uint64_t assigned_id = entry.request.id;
        if (expire_after_.count() > 0) {
            entry.deadline = std::chrono::steady_clock::now() + expire_after_;
        }
        queue_.push_back(std::move(entry));
        const bool high_watermark =
            queue_.size() >= high_watermark_ && !high_watermark_reported_;
        if (high_watermark) {
            high_watermark_reported_ = true;
        }
        return {true, high_watermark, assigned_id};
    }

    struct DrainStats {
        std::size_t executed{0};
        std::size_t expired{0};
        std::size_t failed{0};
    };

    // 业务单写者侧：单拍至多 max_per_pump 条。队首过期先丢（计 expired，
    // 不占消费预算）；业务抛异常计 failed，链不断（同拍余量继续）。
    DrainStats drain(const std::function<void(const CallbackRequest&)>& business) {
        DrainStats stats;
        for (std::size_t budget = max_per_pump_; budget > 0; --budget) {
            std::optional<CallbackRequest> item;
            {
                std::lock_guard<std::mutex> lock(mutex_);
                while (!queue_.empty()) {
                    if (expire_after_.count() > 0 &&
                        std::chrono::steady_clock::now() > queue_.front().deadline) {
                        queue_.pop_front();
                        ++expired_;
                        ++stats.expired;
                        continue;  // 丢弃不占消费预算
                    }
                    auto entry = std::move(queue_.front());
                    queue_.pop_front();
                    if (queue_.size() < high_watermark_) {
                        high_watermark_reported_ = false;  // 水位回落，允许再告
                    }
                    item = std::move(entry.request);
                    break;
                }
            }
            if (!item) {
                break;
            }
            try {
                business(*item);
                ++executed_;
                ++stats.executed;
            } catch (const std::exception&) {
                ++failed_;
                ++stats.failed;
            }
        }
        return stats;
    }

    std::size_t size() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    std::size_t cap() const { return cap_; }
    std::size_t high_watermark() const { return high_watermark_; }
    std::size_t max_per_pump() const { return max_per_pump_; }
    std::size_t rejected_count() const {
        return rejected_.load(std::memory_order_relaxed);
    }
    std::size_t executed_count() const {
        return executed_.load(std::memory_order_relaxed);
    }
    std::size_t expired_count() const {
        return expired_.load(std::memory_order_relaxed);
    }
    std::size_t failed_count() const {
        return failed_.load(std::memory_order_relaxed);
    }

private:
    struct Entry {
        CallbackRequest request;
        std::chrono::steady_clock::time_point deadline{};
    };

    mutable std::mutex mutex_;
    std::deque<Entry> queue_;
    std::size_t cap_;
    std::size_t high_watermark_;
    std::size_t max_per_pump_;
    std::chrono::milliseconds expire_after_;
    bool high_watermark_reported_{false};  // guarded by mutex_
    std::uint64_t next_id_{1};
    std::atomic<std::size_t> rejected_{0};
    std::atomic<std::size_t> executed_{0};
    std::atomic<std::size_t> expired_{0};
    std::atomic<std::size_t> failed_{0};
};

}  // namespace interfaces
