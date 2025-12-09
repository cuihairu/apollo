#include "apollo/net/reactor.hpp"
#include <algorithm>

#ifdef _WIN32
    #include <mswsock.h>
    #include <windows.h>
#else
    #include <sys/epoll.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <errno.h>
#endif

namespace apollo::net {

// Reactor 基类实现
Reactor::Reactor() {
#ifdef _WIN32
    iocpHandle_ = nullptr;
#else
    epollFd_ = -1;
    events_ = nullptr;
#endif
}

Reactor::~Reactor() {
    Shutdown();
}

bool Reactor::Initialize() {
#ifdef _WIN32
    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (iocpHandle_ == nullptr) {
        return false;
    }
    return IOCPReactor::Initialize();
#else
    epollFd_ = epoll_create1(EPOLL_CLOEXEC);
    if (epollFd_ < 0) {
        return false;
    }

    events_ = new epoll_event[MAX_EVENTS];
    return EpollReactor::Initialize();
#endif
}

void Reactor::Shutdown() {
    Stop();

#ifdef _WIN32
    if (iocpHandle_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
    }
#else
    if (epollFd_ >= 0) {
        close(epollFd_);
        epollFd_ = -1;
    }
    if (events_) {
        delete[] events_;
        events_ = nullptr;
    }
#endif
}

bool Reactor::AddSocket(socket_t sockfd, NetEventType events, EventCallback callback) {
    std::lock_guard<std::mutex> lock(socketMutex_);

    SocketInfo info;
    info.sockfd = sockfd;
    info.events = events;
    info.callback = callback;

    socketMap_[sockfd] = info;

#ifdef _WIN32
    // 关联到IOCP
    HANDLE handle = CreateIoCompletionPort((HANDLE)sockfd, iocpHandle_, (ULONG_PTR)sockfd, 0);
    if (handle == nullptr) {
        socketMap_.erase(sockfd);
        return false;
    }

    // 根据事件类型投递IO请求
    if (events & NetEventType::READ) {
        StartReceive(sockfd);
    }
    if (events & NetEventType::WRITE) {
        StartSend(sockfd);
    }
#else
    struct epoll_event ev;
    ev.data.fd = sockfd;
    ev.events = 0;

    if (events & NetEventType::READ) {
        ev.events |= EPOLLIN | EPOLLRDHUP;
    }
    if (events & NetEventType::WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.events |= EPOLLET;  // 边缘触发模式

    if (epoll_ctl(epollFd_, EPOLL_CTL_ADD, sockfd, &ev) < 0) {
        socketMap_.erase(sockfd);
        return false;
    }
#endif

    return true;
}

bool Reactor::ModifySocket(socket_t sockfd, NetEventType events) {
    std::lock_guard<std::mutex> lock(socketMutex_);

    auto it = socketMap_.find(sockfd);
    if (it == socketMap_.end()) {
        return false;
    }

    it->second.events = events;

#ifdef _WIN32
    // IOCP不需要修改，直接重新投递IO请求
    if (events & NetEventType::READ) {
        StartReceive(sockfd);
    }
    if (events & NetEventType::WRITE) {
        StartSend(sockfd);
    }
#else
    struct epoll_event ev;
    ev.data.fd = sockfd;
    ev.events = 0;

    if (events & NetEventType::READ) {
        ev.events |= EPOLLIN | EPOLLRDHUP;
    }
    if (events & NetEventType::WRITE) {
        ev.events |= EPOLLOUT;
    }
    ev.events |= EPOLLET;

    if (epoll_ctl(epollFd_, EPOLL_CTL_MOD, sockfd, &ev) < 0) {
        return false;
    }
#endif

    return true;
}

bool Reactor::RemoveSocket(socket_t sockfd) {
    std::lock_guard<std::mutex> lock(socketMutex_);

    auto it = socketMap_.find(sockfd);
    if (it == socketMap_.end()) {
        return false;
    }

#ifdef _WIN32
    // IOCP会自动处理
#else
    if (epoll_ctl(epollFd_, EPOLL_CTL_DEL, sockfd, nullptr) < 0) {
        // 忽略错误
    }
#endif

    socketMap_.erase(it);
    return true;
}

void Reactor::Stop() {
    running_ = false;
    stopped_ = true;

#ifdef _WIN32
    // 关闭所有worker线程
    for (auto& thread : workerThreads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    workerThreads_.clear();

    // 发送退出信号
    for (size_t i = 0; i < workerThreads_.size(); ++i) {
        PostQueuedCompletionStatus(iocpHandle_, 0, 0, nullptr);
    }
#else
    // Epoll会在下一次epoll_wait时退出
#endif
}

#ifdef _WIN32
// IOCPReactor 实现
bool IOCPReactor::Initialize() {
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    // 创建工作线程
    DWORD threadCount = sysInfo.dwNumberOfProcessors;
    for (DWORD i = 0; i < threadCount; ++i) {
        workerThreads_.emplace_back([this]() {
            DWORD bytesTransferred;
            ULONG_PTR completionKey;
            LPOVERLAPPED overlapped;

            while (running_) {
                BOOL success = GetQueuedCompletionStatus(
                    iocpHandle_,
                    &bytesTransferred,
                    &completionKey,
                    &overlapped,
                    INFINITE
                );

                if (!running_) {
                    break;
                }

                if (overlapped) {
                    OverlappedData* data = (OverlappedData*)overlapped;
                    data->bytesTransferred = bytesTransferred;
                    HandleCompletion(data);
                    delete data;
                }
            }
        });
    }

    return true;
}

void IOCPReactor::StartReceive(socket_t sockfd) {
    OverlappedData* data = new OverlappedData();
    ZeroMemory(&data->overlapped, sizeof(data->overlapped));
    data->sockfd = sockfd;
    data->eventType = NetEventType::READ;
    data->buffer.EnsureWritable(4096);

    DWORD flags = 0;
    DWORD bytesReceived = 0;
    WSABUF wsabuf;
    wsabuf.buf = (char*)data->buffer.Data();
    wsabuf.len = (ULONG)data->buffer.WritableSize();

    int result = WSARecv(
        (SOCKET)sockfd,
        &wsabuf,
        1,
        &bytesReceived,
        &flags,
        &data->overlapped,
        nullptr
    );

    if (result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING) {
        delete data;
    }
}

void IOCPReactor::StartSend(socket_t sockfd) {
    // 实现发送逻辑
}

void IOCPReactor::HandleCompletion(OverlappedData* data) {
    std::lock_guard<std::mutex> lock(socketMutex_);

    auto it = socketMap_.find(data->sockfd);
    if (it == socketMap_.end()) {
        return;
    }

    if (data->eventType == NetEventType::READ) {
        // 继续投递接收请求
        StartReceive(data->sockfd);
    }

    // 调用回调
    it->second.callback(data->sockfd, data->eventType);
}
#else
// EpollReactor 实现
bool EpollReactor::Initialize() {
    return true;
}

bool EpollReactor::EventLoop() {
    running_ = true;

    while (running_) {
        int nfds = epoll_wait(epollFd_, events_, MAX_EVENTS, 1000);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            return false;
        }

        for (int i = 0; i < nfds; ++i) {
            struct epoll_event& ev = events_[i];
            socket_t sockfd = ev.data.fd;

            NetEventType events = NetEventType::NONE;
            if (ev.events & EPOLLIN) {
                events |= NetEventType::READ;
            }
            if (ev.events & EPOLLOUT) {
                events |= NetEventType::WRITE;
            }
            if (ev.events & (EPOLLHUP | EPOLLRDHUP)) {
                events |= NetEventType::CLOSE;
            }
            if (ev.events & EPOLLERR) {
                events |= NetEventType::ERROR;
            }

            // 调用回调
            std::lock_guard<std::mutex> lock(socketMutex_);
            auto it = socketMap_.find(sockfd);
            if (it != socketMap_.end()) {
                it->second.callback(sockfd, events);
            }
        }
    }

    return true;
}
#endif

}  // namespace apollo::net