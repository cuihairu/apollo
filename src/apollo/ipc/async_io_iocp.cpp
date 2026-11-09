/**
 * @file async_io.cpp
 * @brief 跨平台异步 I/O 实现 (IOCP / io_uring / epoll / kqueue)
 */

#include "apollo/ipc/async_io.h"
#include <algorithm>
#include <iostream>

#ifdef _WIN32
    #pragma comment(lib, "ws2_32.lib")
#endif

namespace apollo {
namespace ipc {

//==============================================================================
// IOCP 实现 (Windows)
//==============================================================================

#ifdef APOLLO_IO_IOCP

IocpMultiplexer::IocpMultiplexer(size_t maxEvents)
    : maxEvents_(maxEvents) {
}

IocpMultiplexer::~IocpMultiplexer() {
    stop();
}

bool IocpMultiplexer::start() {
    if (running_) {
        return true;
    }

    // 创建完成端口
    iocpHandle_ = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);
    if (!iocpHandle_) {
        return false;
    }

    // 创建唤醒事件
    wakeupEvent_ = CreateEvent(nullptr, TRUE, FALSE, nullptr);
    if (!wakeupEvent_) {
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
        return false;
    }

    // 注册唤醒事件到完成端口
    HANDLE result = CreateIoCompletionPort(wakeupEvent_, iocpHandle_, 1, 0);
    if (!result) {
        CloseHandle(wakeupEvent_);
        CloseHandle(iocpHandle_);
        return false;
    }

    running_ = true;
    return true;
}

void IocpMultiplexer::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    SetEvent(wakeupEvent_);

    if (iocpHandle_) {
        // 等待一段时间让操作完成
        Sleep(100);
        CloseHandle(iocpHandle_);
        iocpHandle_ = nullptr;
    }

    if (wakeupEvent_) {
        CloseHandle(wakeupEvent_);
        wakeupEvent_ = nullptr;
    }
}

bool IocpMultiplexer::registerFd(int fd, uint32_t events) {
    // 在 Windows 上，fd 实际上是 SOCKET
    HANDLE result = CreateIoCompletionPort(
        reinterpret_cast<HANDLE>(fd),
        iocpHandle_,
        reinterpret_cast<ULONG_PTR>(fd),
        0
    );
    return result != nullptr;
}

bool IocpMultiplexer::unregisterFd(int fd) {
    // IOCP 不需要显式注销，关闭 fd 即可
    (void)fd;
    return true;
}

bool IocpMultiplexer::modifyFd(int fd, uint32_t events) {
    // IOCP 通过 post 操作自动处理
    (void)fd;
    (void)events;
    return true;
}

bool IocpMultiplexer::postRead(int fd, void* buffer, size_t size,
                                IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;
    op.buffer.resize(size);

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    DWORD flags = 0;
    WSABUF buf;
    buf.buf = reinterpret_cast<char*>(iocpOp->buffer.data());
    buf.len = static_cast<ULONG>(size);

    int result = WSARecv(
        reinterpret_cast<SOCKET>(fd),
        &buf, 1,
        nullptr,
        &iocpOp->overlapped,
        nullptr
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postWrite(int fd, const void* data, size_t size,
                                 IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;
    op.buffer.assign(static_cast<const uint8_t*>(data),
                     static_cast<const uint8_t*>(data) + size);

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    WSABUF buf;
    buf.buf = reinterpret_cast<char*>(iocpOp->buffer.data());
    buf.len = static_cast<ULONG>(size);

    int result = WSASend(
        reinterpret_cast<SOCKET>(fd),
        &buf, 1,
        nullptr,
        &iocpOp->overlapped,
        nullptr
    );

    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postAccept(int listenFd,
                                 IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = listenFd;
    op.userData = userData;

    IocpOperation* iocpOp = createOp(listenFd, op);
    if (!iocpOp) {
        return false;
    }

    iocpOp->accept = true;
    iocpOp->acceptSocket = WSASocket(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);

    DWORD bytesReceived = 0;
    BOOL result = AcceptEx(
        reinterpret_cast<SOCKET>(listenFd),
        iocpOp->acceptSocket,
        iocpOp->acceptAddrBuf,
        sizeof(iocpOp->acceptAddrBuf) - 16,
        0,
        sizeof(iocpOp->acceptAddrBuf) / 2,
        &bytesReceived,
        &iocpOp->overlapped
    );

    if (!result) {
        int error = WSAGetLastError();
        if (error != ERROR_IO_PENDING) {
            closesocket(iocpOp->acceptSocket);
            delete iocpOp;
            return false;
        }
    }

    pendingOps_++;
    return true;
}

bool IocpMultiplexer::postConnect(int fd, const std::string& address, uint16_t port,
                                  IoCallback callback, void* userData) {
    AsyncOp op;
    op.callback = std::move(callback);
    op.fd = fd;
    op.userData = userData;

    IocpOperation* iocpOp = createOp(fd, op);
    if (!iocpOp) {
        return false;
    }

    // ConnectEx 需要函数指针动态加载
    // 简化实现：这里使用同步方式，实际项目中应使用 ConnectEx
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, address.c_str(), &addr.sin_addr);

    int result = connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr));
    if (result == SOCKET_ERROR) {
        int error = WSAGetLastError();
        if (error != WSAEWOULDBLOCK) {
            delete iocpOp;
            IoEvent event{};
            event.userData = userData;
            event.result = -1;
            event.error = error;
            callback(event);
            return false;
        }
    }

    // 异步连接成功
    pendingOps_++;
    return true;
}

uint64_t IocpMultiplexer::scheduleTimer(uint32_t delayMs, std::function<void()> callback) {
    // 创建定时器队列
    uint64_t timerId = nextTimerId_++;

    // 使用 Windows Timer Queue
    HANDLE timerHandle = CreateWaitableTimer(nullptr, FALSE, nullptr);
    if (!timerHandle) {
        return 0;
    }

    LARGE_INTEGER dueTime;
    dueTime.QuadPart = -static_cast<int64_t>(delayMs * 10000);  // 100ns 单位

    if (!SetWaitableTimer(timerHandle, &dueTime, 0, nullptr, nullptr, FALSE)) {
        CloseHandle(timerHandle);
        return 0;
    }

    // 在独立线程中处理定时器（简化实现）
    std::thread([this, timerId, timerHandle, callback]() {
        WaitForSingleObject(timerHandle, INFINITE);
        CloseHandle(timerHandle);
        if (running_) {
            callback();
        }
    }).detach();

    return timerId;
}

bool IocpMultiplexer::cancelTimer(uint64_t timerId) {
    // 简化实现：不支持取消
    (void)timerId;
    return false;
}

void IocpMultiplexer::runOnce(uint32_t timeoutMs) {
    DWORD bytesTransferred = 0;
    ULONG_PTR completionKey = 0;
    LPOVERLAPPED overlapped = nullptr;

    BOOL result = GetQueuedCompletionStatus(
        iocpHandle_,
        &bytesTransferred,
        &completionKey,
        &overlapped,
        static_cast<DWORD>(timeoutMs)
    );

    if (!result) {
        DWORD error = GetLastError();
        if (error == WAIT_TIMEOUT) {
            return;  // 超时
        }
    }

    if (overlapped) {
        processCompletion(bytesTransferred, completionKey, overlapped);
    }
}

void IocpMultiplexer::run() {
    while (running_) {
        runOnce(100);  // 100ms 超时
    }
}

bool IocpMultiplexer::processCompletion(DWORD bytesTransferred, ULONG_PTR completionKey,
                                       LPOVERLAPPED overlapped) {
    IocpOperation* iocpOp = CONTAINING_RECORD(overlapped, IocpOperation, overlapped);

    IoEvent event;
    event.userData = iocpOp->asyncOp.userData;
    event.result = bytesTransferred;
    event.error = 0;

    if (iocpOp->accept) {
        event.operation = IoOp::Accept;
    } else {
        event.operation = (bytesTransferred == 0) ? IoOp::Close : IoOp::Read;
    }

    if (iocpOp->asyncOp.callback) {
        iocpOp->asyncOp.callback(event);
    }

    delete iocpOp;
    processedOps_++;
    pendingOps_--;

    return true;
}

IocpMultiplexer::IocpOperation* IocpMultiplexer::createOp(int fd, const AsyncOp& asyncOp) {
    auto* op = new IocpOperation();
    op->asyncOp = asyncOp;
    op->fd = asyncOp.fd;
    op->buffer = asyncOp.buffer;
    ZeroMemory(&op->overlapped, sizeof(op->overlapped));
    op->accept = false;

    return op;
}

#endif // APOLLO_IO_IOCP

} // namespace ipc
} // namespace apollo
