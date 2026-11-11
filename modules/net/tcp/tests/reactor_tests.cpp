#include "apollo/network/transport/reactor.hpp"
#include "apollo/network/transport/socket.hpp"

#include <atomic>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

using apollo::net::NetAddress;
using apollo::net::NetError;
using apollo::net::NetEventType;
using apollo::net::Reactor;
using apollo::net::Socket;

// 回环建连：listener(端口 0) + 客户端阻塞 connect + 服务端 Accept
bool make_connected_pair(Socket& listener, Socket& client,
                         std::unique_ptr<Socket>& server_conn,
                         uint16_t& port) {
    if (!listener.CreateTCP() || !listener.SetReuseAddr(true)) {
        return false;
    }
    if (!listener.Bind(NetAddress("127.0.0.1", 0)) || !listener.Listen(8)) {
        return false;
    }
    port = listener.GetLocalAddress().GetPort();

    std::thread accept_thread([&listener, &server_conn]() {
        server_conn = listener.Accept();
    });

    if (!client.CreateTCP() || !client.Connect(NetAddress("127.0.0.1", port))) {
        accept_thread.join();
        return false;
    }
    accept_thread.join();
    return server_conn && server_conn->IsValid();
}

template <typename Pred>
bool wait_for(Pred&& pred, std::chrono::seconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (pred()) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return false;
}

bool test_reactor_write_then_read_events() {
    Socket listener, client;
    std::unique_ptr<Socket> server_conn;
    uint16_t port = 0;
    if (!make_connected_pair(listener, client, server_conn, port)) {
        return false;
    }

    Reactor reactor;
    if (!reactor.Initialize()) {
        return false;
    }

    std::atomic<bool> write_fired{false};
    std::atomic<bool> read_fired{false};
    std::atomic<socket_t> read_fd{-1};
    std::atomic<char> read_byte{0};
    const socket_t conn_fd = server_conn->GetFd();

    // 1) WRITE 事件：已建立连接的发送缓冲即刻可写，回调应立即触发
    if (!reactor.AddSocket(conn_fd, NetEventType::WRITE,
                           [&](socket_t, NetEventType events) {
                               if ((events & NetEventType::WRITE) != NetEventType::NONE) {
                                   write_fired = true;
                               }
                           })) {
        return false;
    }

    std::thread loop([&reactor]() { reactor.EventLoop(); });

    if (!wait_for([&] { return write_fired.load(); }, std::chrono::seconds(5))) {
        reactor.Stop();
        loop.join();
        reactor.Shutdown();
        return false;
    }

    // 2) Modify 切 READ：对端写入后 READ 回调触发并读得数据
    if (!reactor.ModifySocket(conn_fd, NetEventType::READ)) {
        reactor.Stop();
        loop.join();
        reactor.Shutdown();
        return false;
    }

    if (!client.SendAll("z", 1)) {
        reactor.Stop();
        loop.join();
        reactor.Shutdown();
        return false;
    }

    reactor.AddSocket(conn_fd, NetEventType::READ,
                      [&](socket_t fd, NetEventType events) {
                          if ((events & NetEventType::READ) != NetEventType::NONE) {
                              char byte = 0;
                              size_t got = 0;
                              if (server_conn->Receive(&byte, 1, got) ==
                                      NetError::SUCCESS &&
                                  got == 1) {
                                  read_fd = fd;
                                  read_byte = byte;
                                  read_fired = true;
                              }
                          }
                      });

    const bool read_ok = wait_for([&] { return read_fired.load(); },
                                  std::chrono::seconds(5));

    reactor.Stop();
    loop.join();
    reactor.Shutdown();

    const bool ok = read_ok && read_fired.load() &&
                    read_fd.load() == conn_fd && read_byte.load() == 'z' &&
                    !reactor.IsRunning();

    listener.Close();
    client.Close();
    return ok;
}

bool test_reactor_remove_and_unknown_fd() {
    Reactor reactor;
    if (!reactor.Initialize()) {
        return false;
    }

    Socket s;
    if (!s.CreateTCP()) {
        return false;
    }

    auto noop = [](socket_t, NetEventType) {};
    if (!reactor.AddSocket(s.GetFd(), NetEventType::READ, noop)) {
        return false;
    }
    // 重复添加同 fd：map 覆盖语义，仍成功
    if (!reactor.AddSocket(s.GetFd(), NetEventType::WRITE, noop)) {
        return false;
    }

    if (!reactor.RemoveSocket(s.GetFd())) {
        return false;
    }
    if (reactor.RemoveSocket(s.GetFd())) {   // 二次移除应失败
        return false;
    }
    if (reactor.ModifySocket(s.GetFd(), NetEventType::READ)) {  // 未知 fd 应失败
        return false;
    }

    s.Close();
    reactor.Shutdown();
    return !reactor.IsRunning();
}

bool test_reactor_empty_loop_stop() {
    Reactor reactor;
    if (!reactor.Initialize() || reactor.IsRunning()) {
        return false;
    }

    // 空 fd 集：循环走 10ms idle 睡眠分支，Stop 后收敛退出
    std::thread loop([&reactor]() { reactor.EventLoop(); });
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    if (!reactor.IsRunning()) {
        reactor.Stop();
        loop.join();
        return false;
    }

    reactor.Stop();
    loop.join();
    reactor.Shutdown();
    return !reactor.IsRunning();
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"reactor_write_then_read_events", test_reactor_write_then_read_events},
        {"reactor_remove_and_unknown_fd", test_reactor_remove_and_unknown_fd},
        {"reactor_empty_loop_stop", test_reactor_empty_loop_stop},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... ";
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "apollo_net_reactor_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "apollo_net_reactor_tests passed" << std::endl;
    return 0;
}
