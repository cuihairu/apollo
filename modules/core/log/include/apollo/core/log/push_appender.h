#pragma once

#include "apollo/core/log/appender.h"
#include "apollo/core/log/structured.h"

#include <functional>

namespace apollo {
namespace core {
namespace log {

/**
 * @brief push 出口 appender（L4：logging.md §5 双出口的第二出口）
 *
 * append 在 logger 调用线程发生（任意线程可达）——行格式化后交注入的
 * 传输 sink。本类不知网络、不缓冲、不阻塞：断连降级语义（有界队列 +
 * 顶格丢新 + 重连补投）归传输 sink 承担，收集永不构成运行依赖。
 */
class PushAppender : public IAppender {
public:
    using Sink = std::function<void(const std::string& structured_line)>;

    explicit PushAppender(Sink sink) : sink_(std::move(sink)) {}

    void append(const LogRecord& record) override {
        if (!sink_) {
            return;
        }
        try {
            sink_(to_structured_line(record));
        } catch (...) {
            // sink 异常 = 收集面故障：吞掉，本地文件仍是真相源
        }
    }

    void flush() override {}

private:
    Sink sink_;
};

} // namespace log
} // namespace core
} // namespace apollo
