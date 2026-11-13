#include "apollo/network/transport/socket.hpp"

#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

using apollo::net::Buffer;
using apollo::net::NetAddress;
using apollo::net::NetError;
using apollo::net::Socket;

bool test_net_address_basic() {
    NetAddress addr("127.0.0.1", 12345);
    if (addr.GetIP() != "127.0.0.1" || addr.GetPort() != 12345) {
        return false;
    }

    const std::string text = addr.ToString();
    if (text.find("127.0.0.1") == std::string::npos ||
        text.find("12345") == std::string::npos) {
        return false;
    }

    NetAddress unset;
    return unset.GetPort() == 0;
}

bool test_socket_create_and_options() {
    Socket s;
    if (s.IsValid()) {
        return false;
    }

    if (!s.CreateTCP() || !s.IsValid()) {
        return false;
    }
    if (!s.SetNonBlocking(false) || !s.SetNonBlocking(true) || !s.SetNonBlocking(false)) {
        return false;
    }
    if (!s.SetTcpNoDelay(true) || !s.SetReuseAddr(true)) {
        return false;
    }
    if (!s.SetRecvBufferSize(64 * 1024) || !s.SetSendBufferSize(64 * 1024)) {
        return false;
    }

    Socket udp;
    if (!udp.CreateUDP() || !udp.IsValid()) {
        return false;
    }

    return true;
}

bool test_buffer_read_write_semantics() {
    Buffer buf;

    const char* msg = "hello";
    buf.Write(msg, 5);
    if (buf.GetReadableSize() != 5) {
        return false;
    }

    // Peek 不推进读取位
    if (std::memcmp(buf.Peek(), "hello", 5) != 0 || buf.GetReadableSize() != 5) {
        return false;
    }

    char out[8] = {};
    if (buf.Read(out, 8) != 5 || std::memcmp(out, "hello", 5) != 0) {
        return false;
    }
    // 读尽后指针复位，再读得 0
    if (buf.Read(out, 1) != 0 || buf.GetReadableSize() != 0) {
        return false;
    }

    // Discard + Compact：保留未读段
    buf.Write("abcdef", 6);
    buf.Discard(2);                 // 丢 "ab"，剩 "cdef"
    buf.Compact();
    if (buf.GetReadableSize() != 4 || std::memcmp(buf.Peek(), "cdef", 4) != 0) {
        return false;
    }

    buf.Clear();
    if (buf.GetReadableSize() != 0) {
        return false;
    }

    // EnsureWritable 扩容
    buf.EnsureWritable(1024);
    return buf.GetWritableSize() >= 1024;
}

bool test_tcp_echo_loopback() {
    Socket listener;
    if (!listener.CreateTCP() || !listener.SetReuseAddr(true)) {
        return false;
    }
    // 端口 0：由内核分配，经 GetLocalAddress 取回
    if (!listener.Bind(NetAddress("127.0.0.1", 0)) || !listener.Listen(8)) {
        return false;
    }
    const uint16_t port = listener.GetLocalAddress().GetPort();
    if (port == 0) {
        return false;
    }

    bool server_ok = false;
    std::thread server([&listener, &server_ok]() {
        std::unique_ptr<Socket> conn = listener.Accept();
        if (!conn || !conn->IsValid()) {
            return;
        }
        char rx[16] = {};
        if (!conn->ReceiveAll(rx, 4)) {
            return;
        }
        if (!conn->SendAll(rx, 4)) {
            return;
        }
        server_ok = true;
    });

    Socket client;
    if (!client.CreateTCP() || !client.Connect(NetAddress("127.0.0.1", port))) {
        server.join();
        return false;
    }
    if (!client.IsConnected() ||
        client.GetRemoteAddress().GetPort() != port) {
        server.join();
        return false;
    }

    if (!client.SendAll("ping", 4)) {
        server.join();
        return false;
    }

    char echo[16] = {};
    bool received = client.ReceiveAll(echo, 4);
    server.join();

    return received && std::memcmp(echo, "ping", 4) == 0 && server_ok;
}

bool test_receive_after_peer_close() {
    Socket listener;
    if (!listener.CreateTCP() || !listener.SetReuseAddr(true) ||
        !listener.Bind(NetAddress("127.0.0.1", 0)) || !listener.Listen(8)) {
        return false;
    }
    const uint16_t port = listener.GetLocalAddress().GetPort();

    std::unique_ptr<Socket> conn;
    std::thread server([&listener, &conn]() {
        conn = listener.Accept();
    });

    Socket client;
    if (!client.CreateTCP() || !client.Connect(NetAddress("127.0.0.1", port))) {
        server.join();
        return false;
    }
    // 对端非阻塞轮询，避免依赖阻塞语义的死等
    server.join();
    if (!conn) {
        return false;
    }
    conn->SetNonBlocking(true);

    client.SendAll("x", 1);
    client.Close();   // 发完即关：数据先达，随后 EOF

    // TCP 流序保证：先收到 1 字节，再读到关闭（EOF → SOCKET_CLOSED）
    char byte = 0;
    size_t got = 0;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    NetError first = NetError::WOULDBLOCK;
    while (std::chrono::steady_clock::now() < deadline) {
        first = conn->Receive(&byte, 1, got);
        if (first != NetError::WOULDBLOCK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (first != NetError::SUCCESS || got != 1 || byte != 'x') {
        return false;
    }

    NetError closed = NetError::WOULDBLOCK;
    while (std::chrono::steady_clock::now() < deadline) {
        closed = conn->Receive(&byte, 1, got);
        if (closed != NetError::WOULDBLOCK) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    // 关闭后读返回 SOCKET_CLOSED（或非 SUCCESS 非 WOULDBLOCK）
    return closed == NetError::SOCKET_CLOSED ||
           (closed != NetError::SUCCESS && closed != NetError::WOULDBLOCK);
}

// audit C3：对端关闭后的写必须返回错误（EPIPE 面）而非 SIGPIPE 杀进程——
// 本套件不忽略 SIGPIPE（main 无 signal 设置），若 Send 未设防，写拍到断管
// 信号进程即死，用例到不了断言
bool test_send_after_peer_close_no_sigpipe() {
    Socket listener;
    if (!listener.CreateTCP() || !listener.SetReuseAddr(true) ||
        !listener.Bind(NetAddress("127.0.0.1", 0)) || !listener.Listen(8)) {
        return false;
    }
    const uint16_t port = listener.GetLocalAddress().GetPort();

    std::unique_ptr<Socket> conn;
    std::thread server([&listener, &conn]() { conn = listener.Accept(); });

    Socket client;
    if (!client.CreateTCP() || !client.Connect(NetAddress("127.0.0.1", port))) {
        server.join();
        return false;
    }
    server.join();
    if (!conn) {
        return false;
    }

    client.Close();  // 客户端先关：对端写必落断管路径
    conn->SetNonBlocking(true);

    // 首写可能被 TCP 缓冲吸收（FIN 只是半关，RST 未达前 SUCCESS），持续写
    // 直到非 SUCCESS；无 MSG_NOSIGNAL/SO_NOSIGPIPE 时此循环内进程已死
    char junk[64] = {};
    size_t sent = 0;
    NetError err = NetError::WOULDBLOCK;
    for (int i = 0; i < 1000; ++i) {
        err = conn->Send(junk, sizeof(junk), sent);
        if (err != NetError::SUCCESS) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return err == NetError::SOCKET_SEND_FAILED;
}

} // namespace

int main() {
    struct Named {
        const char* name;
        bool (*fn)();
    };
    const Named tests[] = {
        {"net_address_basic", test_net_address_basic},
        {"socket_create_and_options", test_socket_create_and_options},
        {"buffer_read_write_semantics", test_buffer_read_write_semantics},
        {"tcp_echo_loopback", test_tcp_echo_loopback},
        {"receive_after_peer_close", test_receive_after_peer_close},
        {"send_after_peer_close_no_sigpipe", test_send_after_peer_close_no_sigpipe},
    };

    for (const auto& t : tests) {
        std::cout << "Running test_" << t.name << "... ";
        if (t.fn()) {
            std::cout << "PASSED" << std::endl;
        } else {
            std::cout << "FAILED" << std::endl;
            std::cerr << "apollo_net_tcp_tests failed" << std::endl;
            return 1;
        }
    }

    std::cout << "apollo_net_tcp_tests passed" << std::endl;
    return 0;
}
