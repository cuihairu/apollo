#pragma once
//
// push 出口接缝（ADR-013：push 留接口不实现——collector 面 M1 后换
// InterServerLink 底座，net-abstraction §5.7；断连降级语义随 L4 落）。
//
// v1 唯一实现 = 空推：本地结构化文件已是真相源（logging.md §1「本地文件是
// 真相源，收集是优化」），接缝只钉调用位形——摄取路径每解析一行过一次
// sink，L4 换实现不动检索面。三禁红线：任何实现不得开 per-process 端口。

#include "logger/structured_line.hpp"

namespace logger {

class IPushSink {
public:
    virtual ~IPushSink() = default;
    // 摄取回调：每解析出一条结构化行调用一次（过滤前——推面收全量，
    // 过滤是检索面本地关注点）
    virtual void on_record(const StructuredLine& record) = 0;
};

// v1 空推实现：零副作用占位
class NullPushSink final : public IPushSink {
public:
    void on_record(const StructuredLine&) override {}
};

} // namespace logger
