#pragma once

#include <nng/nng.h>
#include <nng/protocol/reqrep0/rep.h>
#include <nng/protocol/reqrep0/req.h>
#include <nng/protocol/pubsub0/pub.h>
#include <nng/protocol/pubsub0/sub.h>
#include <nng/protocol/pair0/pair.h>
#include <nng/supplemental/util/platform.h>
#include <string>
#include <vector>
#include <system_error>
#include <memory>
#include <functional>

namespace apollo {
namespace protocol {

//==============================================================================
// NNG 错误处理
//==============================================================================

class NngError : public std::system_error {
public:
    explicit NngError(int rv)
        : std::system_error(rv, nng_category()) {}

    static std::error_category const& nng_category();
};

//==============================================================================
// NNG RAII 辅助类
//==============================================================================

// Socket RAII 包装
class NngSocket {
public:
    NngSocket() = default;

    explicit NngSocket(int (*)(nng_socket*))
        : owns_(true) {
        int rv = open(&socket_, opener);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    ~NngSocket() {
        close();
    }

    // 禁止拷贝
    NngSocket(const NngSocket&) = delete;
    NngSocket& operator=(const NngSocket&) = delete;

    // 移动
    NngSocket(NngSocket&& other) noexcept
        : socket_(other.socket_), owns_(other.owns_) {
        other.socket_.id = 0;
        other.owns_ = false;
    }

    NngSocket& operator=(NngSocket&& other) noexcept {
        if (this != &other) {
            close();
            socket_ = other.socket_;
            owns_ = other.owns_;
            other.socket_.id = 0;
            other.owns_ = false;
        }
        return *this;
    }

    // 获取原生 socket
    nng_socket get() const { return socket_; }

    // 释放所有权
    nng_socket release() {
        owns_ = false;
        return socket_;
    }

    // 关闭
    void close() {
        if (owns_ && socket_.id != 0) {
            nng_close(socket_);
            socket_.id = 0;
        }
    }

    // 接收消息
    void receive(void* buf, size_t size, nng_flag flag = nng_flag(0)) {
        int rv = nng_recv(socket_, buf, size, flag);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    // 接收消息（动态分配）
    std::vector<uint8_t> receive(nng_flag flag = nng_flag(0)) {
        size_t sz = 0;
        int rv = nng_recv(socket_, nullptr, 0, flag | nng_flag::NNG_FLAG_ALLOC);
        if (rv != 0) {
            throw NngError(rv);
        }
        nng_recv(socket_, &sz, sizeof(sz), nng_flag::NNG_FLAG_ALLOC);
        return std::vector<uint8_t>(sz);
    }

    // 发送消息
    void send(const void* buf, size_t size, nng_flag flag = nng_flag(0)) {
        int rv = nng_send(socket_, buf, size, flag);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    void send(const std::vector<uint8_t>& data, nng_flag flag = nng_flag(0)) {
        send(data.data(), data.size(), flag);
    }

    // 设置选项
    void set_int(const char* name, int value) {
        int rv = nng_socket_set_int(socket_, name, value);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    void set_ms(const char* name, nng_duration value) {
        int rv = nng_socket_set_ms(socket_, name, value);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    void set_string(const char* name, const std::string& value) {
        int rv = nng_socket_set_string(socket_, name, value.c_str());
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    void set_bool(const char* name, bool value) {
        int rv = nng_socket_set_bool(socket_, name, value);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    // 获取选项
    int get_int(const char* name) const {
        int value;
        int rv = nng_socket_get_int(socket_, name, &value);
        if (rv != 0) {
            throw NngError(rv);
        }
        return value;
    }

    bool get_bool(const char* name) const {
        bool value;
        int rv = nng_socket_get_bool(socket_, name, &value);
        if (rv != 0) {
            throw NngError(rv);
        }
        return value;
    }

private:
    int open(nng_socket* sock, int (*opener)(nng_socket*)) {
        return opener(sock);
    }

    nng_socket socket_{NNG_SOCKET_INITIALIZER};
    bool owns_ = false;
};

// Dialer RAII 包装
class NngDialer {
public:
    explicit NngDialer(nng_socket sock, const std::string& addr)
        : dialer_(create_dialer(sock, addr)) {}

    ~NngDialer() {
        if (dialer_.id != 0) {
            nng_dialer_close(dialer_);
        }
    }

    // 移动
    NngDialer(NngDialer&& other) noexcept
        : dialer_(other.dialer_) {
        other.dialer_.id = 0;
    }

    NngDialer& operator=(NngDialer&& other) noexcept {
        if (this != &other) {
            if (dialer_.id != 0) {
                nng_dialer_close(dialer_);
            }
            dialer_ = other.dialer_;
            other.dialer_.id = 0;
        }
        return *this;
    }

    nng_dialer get() const { return dialer_; }

private:
    static nng_dialer create_dialer(nng_socket sock, const std::string& addr) {
        nng_dialer d;
        int rv = nng_dialer(sock, &d, addr.c_str());
        if (rv != 0) {
            throw NngError(rv);
        }
        return d;
    }

    nng_dialer dialer_{NNG_DIALER_INITIALIZER};
};

// Listener RAII 包装
class NngListener {
public:
    explicit NngListener(nng_socket sock, const std::string& addr)
        : listener_(create_listener(sock, addr)) {}

    ~NngListener() {
        if (listener_.id != 0) {
            nng_listener_close(listener_);
        }
    }

    // 移动
    NngListener(NngListener&& other) noexcept
        : listener_(other.listener_) {
        other.listener_.id = 0;
    }

    NngListener& operator=(NngListener&& other) noexcept {
        if (this != &other) {
            if (listener_.id != 0) {
                nng_listener_close(listener_);
            }
            listener_ = other.listener_;
            other.listener_.id = 0;
        }
        return *this;
    }

    nng_listener get() const { return listener_; }

private:
    static nng_listener create_listener(nng_socket sock, const std::string& addr) {
        nng_listener l;
        int rv = nng_listener(sock, &l, addr.c_str());
        if (rv != 0) {
            throw NngError(rv);
        }
        return l;
    }

    nng_listener listener_{NNG_LISTENER_INITIALIZER};
};

// AIO (异步IO) RAII 包装
class NngAio {
public:
    NngAio()
        : aio_(create()) {}

    ~NngAio() {
        if (aio_ != nullptr) {
            nng_aio_free(aio_);
        }
    }

    // 禁止拷贝和移动
    NngAio(const NngAio&) = delete;
    NngAio& operator=(const NngAio&) = delete;
    NngAio(NngAio&&) = delete;
    NngAio& operator=(NngAio&&) = delete;

    nng_aio get() const { return aio_; }

    // 等待完成
    void wait() {
        nng_aio_wait(aio_);
    }

    // 设置回调
    void set_callback(std::function<void(nng_aio*)> cb) {
        callback_ = std::move(cb);
        nng_aio_set_cb(aio_, &NngAio::static_callback, this);
    }

    // 获取结果
    int result() const {
        return nng_aio_result(aio_);
    }

    // 获取消息
    nng_msg* get_msg() const {
        return nng_aio_get_msg(aio_);
    }

    // 设置消息
    void set_msg(nng_msg* msg) {
        nng_aio_set_msg(aio_, msg);
    }

    // 获取输出数据
    size_t get_output_size() const {
        return nng_aio_count(aio_);
    }

    void* get_output_data() const {
        return nng_aio_get_msg(aio_);
    }

private:
    static nng_aio* create() {
        nng_aio* aio;
        int rv = nng_aio_alloc(&aio, nullptr, nullptr);
        if (rv != 0) {
            throw NngError(rv);
        }
        return aio;
    }

    static void static_callback(void* arg) {
        auto* self = static_cast<NngAio*>(arg);
        if (self->callback_) {
            self->callback_(self->aio_);
        }
    }

    nng_aio* aio_;
    std::function<void(nng_aio*)> callback_;
};

// Message RAII 包装
class NngMsg {
public:
    NngMsg() : msg_(nullptr) {}

    explicit NngMsg(size_t size) : msg_(create(size)) {}

    ~NngMsg() {
        if (msg_ != nullptr) {
            nng_msg_free(msg_);
        }
    }

    // 禁止拷贝
    NngMsg(const NngMsg&) = delete;
    NngMsg& operator=(const NngMsg&) = delete;

    // 移动
    NngMsg(NngMsg&& other) noexcept
        : msg_(other.msg_) {
        other.msg_ = nullptr;
    }

    NngMsg& operator=(NngMsg&& other) noexcept {
        if (this != &other) {
            if (msg_ != nullptr) {
                nng_msg_free(msg_);
            }
            msg_ = other.msg_;
            other.msg_ = nullptr;
        }
        return *this;
    }

    nng_msg* get() const { return msg_; }

    // 释放所有权
    nng_msg* release() {
        auto* tmp = msg_;
        msg_ = nullptr;
        return tmp;
    }

    // 获取/设置数据
    void* body() {
        return nng_msg_body(msg_);
    }

    const void* body() const {
        return nng_msg_body(msg_);
    }

    size_t len() const {
        return nng_msg_len(msg_);
    }

    void append(const void* data, size_t size) {
        int rv = nng_msg_append(msg_, data, size);
        if (rv != 0) {
            throw NngError(rv);
        }
    }

    void append(const std::vector<uint8_t>& data) {
        append(data.data(), data.size());
    }

    // 从数据创建
    static NngMsg from_data(const void* data, size_t size) {
        NngMsg msg(size);
        std::memcpy(msg.body(), data, size);
        return msg;
    }

    static NngMsg from_data(const std::vector<uint8_t>& data) {
        return from_data(data.data(), data.size());
    }

    // 转换为 vector
    std::vector<uint8_t> to_vector() const {
        auto* p = static_cast<const uint8_t*>(body());
        return std::vector<uint8_t>(p, p + len());
    }

private:
    static nng_msg* create(size_t size) {
        nng_msg* msg;
        int rv = nng_msg_alloc(&msg);
        if (rv != 0) {
            throw NngError(rv);
        }
        if (size > 0) {
            rv = nng_msg_reserve(msg, size);
            if (rv != 0) {
                nng_msg_free(msg);
                throw NngError(rv);
            }
        }
        return msg;
    }

    nng_msg* msg_;
};

//==============================================================================
// 辅助函数
//==============================================================================

// 创建地址字符串
inline std::string make_url(const std::string& scheme, const std::string& host, uint16_t port) {
    return scheme + "://" + host + ":" + std::to_string(port);
}

inline std::string make_tcp_url(const std::string& host, uint16_t port) {
    return make_url("tcp", host, port);
}

inline std::string make_ipc_url(const std::string& path) {
    return "ipc://" + path;
}

inline std::string make_inproc_url(const std::string& name) {
    return "inproc://" + name;
}

} // namespace protocol
} // namespace apollo
