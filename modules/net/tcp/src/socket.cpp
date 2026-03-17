#include "apollo/network/transport/socket.hpp"
#include <cstring>
#include <cerrno>
#include <fcntl.h>

#ifdef _WIN32
    #define LAST_ERROR WSAGetLastError()
    #define ERROR_WOULDBLOCK WSAEWOULDBLOCK
#else
    #define LAST_ERROR errno
    #define ERROR_WOULDBLOCK EWOULDBLOCK
#endif

namespace apollo::net {

Socket::Socket() = default;

Socket::Socket(socket_t sockfd, const NetAddress& addr)
    : sockfd_(sockfd), remoteAddr_(addr) {
    connected_ = (sockfd_ != INVALID_SOCKET);
}

Socket::~Socket() {
    Close();
}

Socket::Socket(Socket&& other) noexcept
    : sockfd_(other.sockfd_)
    , localAddr_(other.localAddr_)
    , remoteAddr_(other.remoteAddr_)
    , connected_(other.connected_) {
    other.sockfd_ = INVALID_SOCKET;
    other.connected_ = false;
}

Socket& Socket::operator=(Socket&& other) noexcept {
    if (this != &other) {
        Close();
        sockfd_ = other.sockfd_;
        localAddr_ = other.localAddr_;
        remoteAddr_ = other.remoteAddr_;
        connected_ = other.connected_;
        other.sockfd_ = INVALID_SOCKET;
        other.connected_ = false;
    }
    return *this;
}

bool Socket::CreateTCP() {
    sockfd_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    return sockfd_ != INVALID_SOCKET;
}

bool Socket::CreateUDP() {
    sockfd_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    return sockfd_ != INVALID_SOCKET;
}

bool Socket::Bind(const NetAddress& addr) {
    if (!IsValid()) {
        return false;
    }

    int ret = bind(sockfd_, reinterpret_cast<const sockaddr*>(&addr.GetSockAddr()),
                   sizeof(addr.GetSockAddr()));
    if (ret == 0) {
        localAddr_ = addr;
        return true;
    }
    return false;
}

bool Socket::Listen(int backlog) {
    if (!IsValid()) {
        return false;
    }

    return listen(sockfd_, backlog) == 0;
}

std::unique_ptr<Socket> Socket::Accept() {
    if (!IsValid()) {
        return nullptr;
    }

    sockaddr_in clientAddr{};
    socklen_t addrLen = sizeof(clientAddr);
    socket_t clientSock = accept(sockfd_, reinterpret_cast<sockaddr*>(&clientAddr), &addrLen);

    if (clientSock != INVALID_SOCKET) {
        return std::make_unique<Socket>(clientSock, NetAddress(clientAddr));
    }
    return nullptr;
}

bool Socket::Connect(const NetAddress& addr) {
    if (!IsValid()) {
        return false;
    }

    int ret = connect(sockfd_, reinterpret_cast<const sockaddr*>(&addr.GetSockAddr()),
                     sizeof(addr.GetSockAddr()));
    if (ret == 0) {
        remoteAddr_ = addr;
        connected_ = true;
        return true;
    }

    int err = GetLastError();
    return err == ERROR_WOULDBLOCK;
}

NetError Socket::Send(const void* data, size_t len, size_t& sent) {
    if (!IsValid() || !connected_) {
        return NetError::SOCKET_SEND_FAILED;
    }

#ifdef _WIN32
    int ret = send(sockfd_, static_cast<const char*>(data), static_cast<int>(len), 0);
#else
    ssize_t ret = send(sockfd_, data, len, 0);
#endif

    if (ret > 0) {
        sent = static_cast<size_t>(ret);
        return NetError::SUCCESS;
    }

    sent = 0;
    int err = GetLastError();
    if (err == ERROR_WOULDBLOCK) {
        return NetError::WOULDBLOCK;
    }
    return NetError::SOCKET_SEND_FAILED;
}

NetError Socket::Receive(void* buffer, size_t len, size_t& received) {
    if (!IsValid()) {
        return NetError::SOCKET_RECV_FAILED;
    }

#ifdef _WIN32
    int ret = recv(sockfd_, static_cast<char*>(buffer), static_cast<int>(len), 0);
#else
    ssize_t ret = recv(sockfd_, buffer, len, 0);
#endif

    if (ret > 0) {
        received = static_cast<size_t>(ret);
        return NetError::SUCCESS;
    }

    received = 0;
    if (ret == 0) {
        return NetError::SOCKET_CLOSED;
    }

    int err = GetLastError();
    if (err == ERROR_WOULDBLOCK) {
        return NetError::WOULDBLOCK;
    }
    return NetError::SOCKET_RECV_FAILED;
}

bool Socket::SendAll(const void* data, size_t len) {
    size_t totalSent = 0;
    while (totalSent < len) {
        size_t sent = 0;
        NetError err = Send(static_cast<const char*>(data) + totalSent, len - totalSent, sent);
        if (err != NetError::SUCCESS) {
            return false;
        }
        totalSent += sent;
    }
    return true;
}

bool Socket::ReceiveAll(void* buffer, size_t len) {
    size_t totalReceived = 0;
    while (totalReceived < len) {
        size_t received = 0;
        NetError err = Receive(static_cast<char*>(buffer) + totalReceived, len - totalReceived, received);
        if (err == NetError::SOCKET_CLOSED) {
            return false;
        }
        if (err != NetError::SUCCESS && err != NetError::WOULDBLOCK) {
            return false;
        }
        if (err == NetError::WOULDBLOCK) {
            // 等待数据
            continue;
        }
        totalReceived += received;
    }
    return true;
}

void Socket::Close() {
    if (sockfd_ != INVALID_SOCKET) {
        closesocket(sockfd_);
        sockfd_ = INVALID_SOCKET;
    }
    connected_ = false;
}

bool Socket::SetNonBlocking(bool nonBlocking) {
    if (!IsValid()) {
        return false;
    }

#ifdef _WIN32
    u_long mode = nonBlocking ? 1 : 0;
    return ioctlsocket(sockfd_, FIONBIO, &mode) == 0;
#else
    int flags = fcntl(sockfd_, F_GETFL, 0);
    if (flags == -1) {
        return false;
    }
    flags = nonBlocking ? (flags | O_NONBLOCK) : (flags & ~O_NONBLOCK);
    return fcntl(sockfd_, F_SETFL, flags) == 0;
#endif
}

bool Socket::SetTcpNoDelay(bool noDelay) {
    if (!IsValid()) {
        return false;
    }

    int flag = noDelay ? 1 : 0;
    return setsockopt(sockfd_, IPPROTO_TCP, TCP_NODELAY,
                     reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

bool Socket::SetReuseAddr(bool reuse) {
    if (!IsValid()) {
        return false;
    }

    int flag = reuse ? 1 : 0;
    return setsockopt(sockfd_, SOL_SOCKET, SO_REUSEADDR,
                     reinterpret_cast<const char*>(&flag), sizeof(flag)) == 0;
}

bool Socket::SetRecvBufferSize(size_t size) {
    if (!IsValid()) {
        return false;
    }

    int bufSize = static_cast<int>(size);
    return setsockopt(sockfd_, SOL_SOCKET, SO_RCVBUF,
                     reinterpret_cast<const char*>(&bufSize), sizeof(bufSize)) == 0;
}

bool Socket::SetSendBufferSize(size_t size) {
    if (!IsValid()) {
        return false;
    }

    int bufSize = static_cast<int>(size);
    return setsockopt(sockfd_, SOL_SOCKET, SO_SNDBUF,
                     reinterpret_cast<const char*>(&bufSize), sizeof(bufSize)) == 0;
}

NetAddress Socket::GetLocalAddress() const {
    if (!IsValid()) {
        return NetAddress();
    }

    sockaddr_in addr{};
    socklen_t len = sizeof(addr);
    if (getsockname(sockfd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
        return NetAddress(addr);
    }
    return NetAddress();
}

NetAddress Socket::GetRemoteAddress() const {
    return remoteAddr_;
}

int Socket::GetLastError() const {
    return LAST_ERROR;
}

std::string Socket::GetErrorString() const {
    int err = GetLastError();
    char buffer[256] = {};

#ifdef _WIN32
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   nullptr, err, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                   buffer, sizeof(buffer), nullptr);
#else
    strerror_r(err, buffer, sizeof(buffer));
#endif

    return std::string(buffer);
}

}  // namespace apollo::net