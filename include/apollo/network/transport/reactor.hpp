#pragma once

#include "socket.hpp"
#include <thread>
#include <unordered_map>
#include <atomic>
#include <functional>

namespace apollo::net {

/// 事件回调函数
using EventCallback = std::function<void(socket_t sockfd, NetEventType events)>;

/// 事件反应器 - 跨平台的IO多路复用封装
class Reactor {
public:
    Reactor();
    ~Reactor();

    // 禁止拷贝
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    /// 初始化反应器
    bool Initialize();

    /// 关闭反应器
    void Shutdown();

    /// 添加Socket监听
    bool AddSocket(socket_t sockfd, NetEventType events, EventCallback callback);

    /// 修改Socket监听事件
    bool ModifySocket(socket_t sockfd, NetEventType events);

    /// 移除Socket监听
    bool RemoveSocket(socket_t sockfd);

    /// 事件循环（在独立线程中运行）
    bool EventLoop();

    /// 停止事件循环
    void Stop();

    /// 是否运行中
    bool IsRunning() const { return running_; }

private:
    void HandleEvents();

    struct SocketInfo {
        socket_t sockfd;
        NetEventType events;
        EventCallback callback;
    };

#ifdef _WIN32
    HANDLE iocpHandle_;
    std::vector<std::thread> workerThreads_;
#else
    int epollFd_;
    struct epoll_event* events_;
    static const size_t MAX_EVENTS = 1024;
#endif

    std::unordered_map<socket_t, SocketInfo> socketMap_;
    std::mutex socketMutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
};

#ifdef _WIN32
/// IOCP操作数据
struct OverlappedData {
    WSAOVERLAPPED overlapped;
    socket_t sockfd;
    NetEventType eventType;
    Buffer buffer;
    DWORD bytesTransferred;
};

class IOCPReactor : public Reactor {
public:
    IOCPReactor();
    ~IOCPReactor();

    bool Initialize() override;
    bool AddSocket(socket_t sockfd, NetEventType events, EventCallback callback) override;
    bool EventLoop() override;

private:
    void StartReceive(socket_t sockfd);
    void StartSend(socket_t sockfd);
    void HandleCompletion(OverlappedData* data);
};
#else
/// Epoll反应器
class EpollReactor : public Reactor {
public:
    EpollReactor();
    ~EpollReactor();

    bool Initialize() override;
    bool EventLoop() override;
};
#endif

}  // namespace apollo::net