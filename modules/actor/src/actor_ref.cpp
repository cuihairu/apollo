/**
 * @file actor_ref.cpp
 * @brief Actor 引用实现
 */

#include "apollo/actor/actor_ref.h"
#include "apollo/actor/actor_system.h"
#include "apollo/actor/actor_cell.h"
#include <future>
#include <atomic>

namespace apollo {
namespace actor {

//==============================================================================
// ActorRef 实现
//==============================================================================

void ActorRef::tellMessage(const Message& msg) const {
    if (!isValid()) {
        return;  // 无效引用，静默忽略
    }

    // 本地 Actor
    if (cell_.lock()) {
        auto cell = cell_.lock();
        cell->tell(msg, ActorRef{});  // TODO: 正确的发送者
        return;
    }

    // 远程 Actor - 通过 MessageBus 发送
    // TODO: 实现
}

void ActorRef::askMessage(const Message& msg, int64_t timeoutMs,
                          std::function<void(const Message&)> callback) const {

    if (!isValid() || !callback) {
        return;
    }

    // 创建临时回复 Actor
    // TODO: 实现
}

//==============================================================================
// 临时回复 Actor（用于 ask 模式）
//==============================================================================

namespace {

class TemporaryReplyActor : public Actor {
public:
    TemporaryReplyActor(std::function<void(const Message&)> callback,
                       int64_t timeoutMs)
        : callback_(std::move(callback))
        , timeoutMs_(timeoutMs) {
    }

protected:
    void receive(const Message& msg) override {
        // 收到响应
        if (callback_) {
            callback_(msg);
        }
        callback_ = nullptr;
        context()->stop();
    }

    void onStart() override {
        Actor::onStart();

        // 设置超时定时器
        if (timeoutMs_ > 0) {
            context()->scheduleOnce(timeoutMs_, [this]() {
                if (callback_) {
                    // 发送超时消息
                    // callback_(Message{});
                    callback_ = nullptr;
                }
                context()->stop();
            });
        }
    }

private:
    std::function<void(const Message&)> callback_;
    int64_t timeoutMs_;
};

} // anonymous namespace

//==============================================================================
// Future 实现（简化版）
//==============================================================================

namespace {

template<typename T>
class Promise {
public:
    std::future<T> get_future() {
        return promise_.get_future();
    }

    void set_value(const T& value) {
        promise_.set_value(value);
    }

    void set_exception(std::exception_ptr ex) {
        promise_.set_exception(ex);
    }

private:
    std::promise<T> promise_;
};

} // anonymous namespace

} // namespace actor
} // namespace apollo
