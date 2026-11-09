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
// 平台检测与后端名称
//==============================================================================

const char* IoMultiplexer::getBackendName() {
#if defined(APOLLO_IO_IOCP)
    return "IOCP (Windows I/O Completion Ports)";
#elif defined(APOLLO_IO_URING)
    return "io_uring (Linux async I/O)";
#elif defined(APOLLO_IO_EPOLL)
    return "epoll (Linux compatible)";
#elif defined(APOLLO_IO_KQUEUE)
    return "kqueue (BSD/macOS)";
#else
    return "Unknown";
#endif
}

//==============================================================================
// 工厂方法
//==============================================================================

std::unique_ptr<IoMultiplexer> IoMultiplexer::create(size_t maxEvents) {
#if defined(APOLLO_IO_IOCP)
    return std::make_unique<IocpMultiplexer>(maxEvents);
#elif defined(APOLLO_IO_URING)
    return std::make_unique<IoUringMultiplexer>(maxEvents, maxEvents / 4);
#elif defined(APOLLO_IO_EPOLL)
    return std::make_unique<EpollMultiplexer>(maxEvents);
#elif defined(APOLLO_IO_KQUEUE)
    return std::make_unique<KqueueMultiplexer>(maxEvents);
#else
    #error "No supported IO backend for this platform"
#endif
}

} // namespace ipc
} // namespace apollo

