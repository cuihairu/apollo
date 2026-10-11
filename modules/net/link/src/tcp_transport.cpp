#include "apollo/net/link/tcp_transport.hpp"

#if !defined(_WIN32)

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>

namespace apollo::net::link {

namespace {

int set_nonblocking(int fd) {
    const int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0) {
        return -1;
    }
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

}  // namespace

// ---- TcpTransport ----

TcpTransport::TcpTransport(int fd) : fd_(fd) {
    set_nonblocking(fd_);
    // 交互式小帧流量：Nagle 批量与延迟 ACK 相互作用会造成几十毫秒级
    // 停顿（本批 TCP e2e 门禁实测踩中）——游戏进程间链路默认关 Nagle。
    int one = 1;
    ::setsockopt(fd_, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
}

TcpTransport::~TcpTransport() {
    close();
}

int TcpTransport::read(std::uint8_t* buf, std::size_t cap) {
    if (fd_ < 0) {
        return -1;
    }
    for (;;) {
        const ssize_t n = ::recv(fd_, buf, cap, 0);
        if (n > 0) {
            return static_cast<int>(n);
        }
        if (n == 0) {
            return -1;  // EOF（FIN）——缝语义里对端关闭与错误同形
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
}

int TcpTransport::write(const std::uint8_t* data, std::size_t size) {
    if (fd_ < 0) {
        return -1;
    }
    for (;;) {
        // MSG_NOSIGNAL：宿主泵模型不假设信号面，断管走 -1 返回码。
        const ssize_t n = ::send(fd_, data, size, MSG_NOSIGNAL);
        if (n >= 0) {
            return static_cast<int>(n);  // 可短写——余量链路层重试
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return 0;
        }
        return -1;
    }
}

void TcpTransport::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
}

// ---- TcpConnector ----

TcpConnector::~TcpConnector() {
    cancel();
}

bool TcpConnector::start(const std::string& host, std::uint16_t port, std::uint32_t timeout_ms,
                         std::uint64_t now_ms) {
    cancel();

    ::addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    ::addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 ||
        res == nullptr) {
        state_ = ConnectState::kFailed;
        failure_ = ConnectFailure::kSystemError;  // 名字解析失败归系统错（M1 不细分 DNS）
        return false;
    }

    bool started = false;
    for (::addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        const int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        set_nonblocking(fd);
        if (::connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) {
            fd_ = fd;
            state_ = ConnectState::kConnected;  // 本机回环常见：立即建成
            started = true;
            break;
        }
        if (errno == EINPROGRESS) {
            fd_ = fd;
            state_ = ConnectState::kConnecting;
            deadline_ms_ = now_ms + timeout_ms;
            started = true;
            break;
        }
        classify_failure(errno);
        ::close(fd);
        break;  // 首候选即分类失败——不逐候选吞错（错误是数据）
    }
    ::freeaddrinfo(res);
    return started;
}

void TcpConnector::cancel() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    state_ = ConnectState::kIdle;
}

TcpConnector::PumpResult TcpConnector::pump(std::uint64_t now_ms) {
    if (state_ == ConnectState::kConnected) {
        return PumpResult::kConnected;
    }
    if (state_ != ConnectState::kConnecting) {
        return state_ == ConnectState::kFailed ? PumpResult::kFailed : PumpResult::kInProgress;
    }
    if (now_ms >= deadline_ms_) {
        classify_failure(ETIMEDOUT);
        ::close(fd_);
        fd_ = -1;
        return PumpResult::kFailed;
    }

    ::pollfd pfd{};
    pfd.fd = fd_;
    pfd.events = POLLOUT;
    if (::poll(&pfd, 1, 0) <= 0) {
        return PumpResult::kInProgress;  // 未就绪 / 被信号打断：下个泵周期再看
    }
    int so_err = 0;
    ::socklen_t len = sizeof so_err;
    if (::getsockopt(fd_, SOL_SOCKET, SO_ERROR, &so_err, &len) != 0) {
        so_err = errno;
    }
    if (so_err != 0) {
        classify_failure(so_err);
        ::close(fd_);
        fd_ = -1;
        return PumpResult::kFailed;
    }
    state_ = ConnectState::kConnected;
    return PumpResult::kConnected;
}

std::unique_ptr<ILinkTransport> TcpConnector::take_transport() {
    if (state_ != ConnectState::kConnected || fd_ < 0) {
        return nullptr;
    }
    auto t = std::make_unique<TcpTransport>(fd_);
    fd_ = -1;
    state_ = ConnectState::kIdle;
    return t;
}

void TcpConnector::classify_failure(int err) {
    switch (err) {
        case ECONNREFUSED:
        case ENETUNREACH:
        case EHOSTUNREACH:
        case EADDRNOTAVAIL:
            failure_ = ConnectFailure::kNoSuchPort;
            break;
        case ETIMEDOUT:
            failure_ = ConnectFailure::kTimedOut;
            break;
        default:
            failure_ = ConnectFailure::kSystemError;
            break;
    }
    state_ = ConnectState::kFailed;
}

// ---- TcpListener ----

TcpListener::~TcpListener() {
    close();
}

bool TcpListener::start(const std::string& host, std::uint16_t port) {
    close();

    ::addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;
    ::addrinfo* res = nullptr;
    if (::getaddrinfo(host.c_str(), std::to_string(port).c_str(), &hints, &res) != 0 ||
        res == nullptr) {
        return false;
    }
    bool ok = false;
    for (::addrinfo* ai = res; ai != nullptr; ai = ai->ai_next) {
        const int fd = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (fd < 0) {
            continue;
        }
        int one = 1;
        ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
        if (::bind(fd, ai->ai_addr, ai->ai_addrlen) != 0 || ::listen(fd, 16) != 0) {
            ::close(fd);
            continue;
        }
        set_nonblocking(fd);
        fd_ = fd;
        ok = true;
        break;
    }
    ::freeaddrinfo(res);
    if (!ok) {
        return false;
    }

    ::sockaddr_storage ss{};
    ::socklen_t slen = sizeof ss;
    if (::getsockname(fd_, reinterpret_cast<::sockaddr*>(&ss), &slen) == 0) {
        port_ = ss.ss_family == AF_INET
                    ? ::ntohs(reinterpret_cast<::sockaddr_in*>(&ss)->sin_port)
                    : ::ntohs(reinterpret_cast<::sockaddr_in6*>(&ss)->sin6_port);
    }
    return true;
}

std::unique_ptr<ILinkTransport> TcpListener::accept() {
    if (fd_ < 0) {
        return nullptr;
    }
    for (;;) {
        const int fd = ::accept(fd_, nullptr, nullptr);
        if (fd >= 0) {
            return std::make_unique<TcpTransport>(fd);
        }
        if (errno == EINTR) {
            continue;
        }
        return nullptr;  // EAGAIN 暂无；其余连接级错误下个泵周期再试
    }
}

void TcpListener::close() {
    if (fd_ >= 0) {
        ::close(fd_);
        fd_ = -1;
    }
    port_ = 0;
}

}  // namespace apollo::net::link

#endif  // !_WIN32
