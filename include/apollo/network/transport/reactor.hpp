#pragma once

#include "socket.hpp"
#include <thread>
#include <unordered_map>
#include <atomic>
#include <functional>
#include <mutex>
#ifndef _WIN32
#include <vector>
#include <poll.h>
#endif

namespace apollo::net {

/// 事件回调函数
using EventCallback = std::function<void(socket_t sockfd, NetEventType events)>;

/// 事件反应器 - 跨平台的IO多路复用封装
class Reactor {
public:
    Reactor();
    virtual ~Reactor();

    // 禁止拷贝
    Reactor(const Reactor&) = delete;
    Reactor& operator=(const Reactor&) = delete;

    /// 初始化反应器
    virtual bool Initialize();

    /// 关闭反应器
    virtual void Shutdown();

    /// 添加Socket监听
    bool AddSocket(socket_t sockfd, NetEventType events, EventCallback callback);

    /// 修改Socket监听事件
    bool ModifySocket(socket_t sockfd, NetEventType events);

    /// 移除Socket监听
    bool RemoveSocket(socket_t sockfd);

    /// 事件循环（在独立线程中运行）
    virtual bool EventLoop();

    /// 停止事件循环
    virtual void Stop();

    /// 是否运行中
    bool IsRunning() const { return running_; }

private:
    struct SocketInfo {
        socket_t sockfd;
        NetEventType events;
        EventCallback callback;
    };

    std::unordered_map<socket_t, SocketInfo> socketMap_;
    std::mutex socketMutex_;
    std::atomic<bool> running_{false};
    std::atomic<bool> stopped_{false};
};

}  // namespace apollo::net
