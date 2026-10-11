#pragma once

// 传输缝（net M1 内核 · 增量 4；offload_wire IOffloadTransport 缝纪律同源：
// 链路内核不含 socket，底座替换 = 换 ILinkTransport 实现）。
//
// 缝语义 = 非阻塞字节流（TCP 投影）：
//   read  → >0 字节数 / 0 暂无 / -1 对端关闭或错误（含本端已 close）
//   write → ≥0 已收字节数（可短写，余量由链路层重试）/ -1 错误
//   close 后 read/write 一律 -1。
// 单线程纪律（offload 桩同款）：缝实现不起内部线程，全部由宿主泵驱动。

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <utility>

namespace apollo::net::link {

class ILinkTransport {
public:
    virtual ~ILinkTransport() = default;
    virtual int read(std::uint8_t* buf, std::size_t cap) = 0;
    virtual int write(const std::uint8_t* data, std::size_t size) = 0;
    virtual void close() = 0;
    virtual bool closed() const = 0;
};

// 内存环回底座（测试/同进程装配用）：write 直投对端收缓冲，读侧排空后
// 见对端 close 即 EOF（TCP FIN 语义：先尽收余量，再见 -1）。无队列上限——
// 背压由链路层水位把守，缝不预支（offload 内存桩同口径）。
class LoopbackEndpoint : public ILinkTransport {
public:
    int read(std::uint8_t* buf, std::size_t cap) override {
        if (closed_) {
            return -1;
        }
        if (rx_.empty()) {
            return peer_ == nullptr || peer_->closed_ ? -1 : 0;
        }
        const std::size_t n = cap < rx_.size() ? cap : rx_.size();
        for (std::size_t i = 0; i < n; ++i) {
            buf[i] = rx_.front();
            rx_.pop_front();
        }
        return static_cast<int>(n);
    }

    int write(const std::uint8_t* data, std::size_t size) override {
        if (closed_ || peer_ == nullptr || peer_->closed_) {
            return -1;
        }
        if (size == 0) {
            return 0;
        }
        peer_->rx_.insert(peer_->rx_.end(), data, data + size);
        return static_cast<int>(size);
    }

    void close() override { closed_ = true; }
    bool closed() const override { return closed_; }

private:
    friend std::pair<std::unique_ptr<LoopbackEndpoint>, std::unique_ptr<LoopbackEndpoint>>
    make_loopback_pair();

    std::deque<std::uint8_t> rx_;
    LoopbackEndpoint* peer_ = nullptr;
    bool closed_ = false;
};

inline std::pair<std::unique_ptr<LoopbackEndpoint>, std::unique_ptr<LoopbackEndpoint>>
make_loopback_pair() {
    auto a = std::make_unique<LoopbackEndpoint>();
    auto b = std::make_unique<LoopbackEndpoint>();
    a->peer_ = b.get();
    b->peer_ = a.get();
    return {std::move(a), std::move(b)};
}

}  // namespace apollo::net::link
