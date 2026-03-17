#include "apollo/network/transport/reactor.hpp"

#include <chrono>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <winsock2.h>
#endif

namespace apollo::net {

Reactor::Reactor() = default;

Reactor::~Reactor() {
    Shutdown();
}

bool Reactor::Initialize() {
    return true;
}

void Reactor::Shutdown() {
    Stop();
}

bool Reactor::AddSocket(socket_t sockfd, NetEventType events, EventCallback callback) {
    std::lock_guard<std::mutex> lock(socketMutex_);
    socketMap_[sockfd] = SocketInfo{sockfd, events, std::move(callback)};
    return true;
}

bool Reactor::ModifySocket(socket_t sockfd, NetEventType events) {
    std::lock_guard<std::mutex> lock(socketMutex_);
    auto it = socketMap_.find(sockfd);
    if (it == socketMap_.end()) {
        return false;
    }
    it->second.events = events;
    return true;
}

bool Reactor::RemoveSocket(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(socketMutex_);
    return socketMap_.erase(sockfd) > 0;
}

bool Reactor::EventLoop() {
    running_ = true;
    stopped_ = false;

#ifdef _WIN32
    constexpr short kReadEvent = POLLRDNORM;
    constexpr short kWriteEvent = POLLWRNORM;
    using PollFd = WSAPOLLFD;
#else
    constexpr short kReadEvent = POLLIN;
    constexpr short kWriteEvent = POLLOUT;
    using PollFd = pollfd;
#endif

    while (running_) {
        std::vector<PollFd> fds;
        fds.reserve(128);

        {
            std::lock_guard<std::mutex> lock(socketMutex_);
            for (const auto& [fd, info] : socketMap_) {
                PollFd pfd{};
                pfd.fd = fd;
                pfd.events = 0;

                if (HasEvent(info.events, NetEventType::READ)) {
                    pfd.events |= kReadEvent;
                }
                if (HasEvent(info.events, NetEventType::WRITE)) {
                    pfd.events |= kWriteEvent;
                }

                fds.push_back(pfd);
            }
        }

        if (fds.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

#ifdef _WIN32
        int ready = WSAPoll(fds.data(), static_cast<ULONG>(fds.size()), 1000);
#else
        int ready = poll(fds.data(), fds.size(), 1000);
#endif

        if (ready <= 0) {
            continue;
        }

        for (const auto& pfd : fds) {
            if (pfd.revents == 0) {
                continue;
            }

            NetEventType fired = NetEventType::NONE;
            if (pfd.revents & kReadEvent) {
                fired |= NetEventType::READ;
            }
            if (pfd.revents & kWriteEvent) {
                fired |= NetEventType::WRITE;
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
                fired |= NetEventType::ERROR;
            }

            EventCallback cb;
            {
                std::lock_guard<std::mutex> lock(socketMutex_);
                auto it = socketMap_.find(pfd.fd);
                if (it == socketMap_.end()) {
                    continue;
                }
                cb = it->second.callback;
            }

            if (cb) {
                cb(pfd.fd, fired);
            }
        }
    }

    return true;
}

void Reactor::Stop() {
    running_ = false;
    stopped_ = true;
}

}  // namespace apollo::net

