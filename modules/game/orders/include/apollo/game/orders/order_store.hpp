#pragma once

// 订单/通知可靠受理与持久待办（todo L63 批⑯）。
//
// 受理面契约——「确认收到不等于发货完成」：accept() 以 append+flush 落盘
// 成功才算受理（返回 accepted，调用方可据此回执渠道「已收到」），订单停
// 留在待办态直到 mark_delivered() 持久置墓碑；两态之间进程崩溃由 journal
// 回放恢复待办（已受理未投递的单不丢），渠道同号重发命中幂等墓碑不建
// 第二单。
//
// 身份契约——「稳定玩家/角色身份关联订单」：订单只携带渠道单号（order_id，
// 幂等键）+ 稳定玩家身份（player_id）；不携带、不缓存任何 Base 进程身份
// ——投递时刻的当前归属（地址 + 归属代次）解析在 OrderDelivery
// （order_delivery.hpp），每拍独立查询（不永久绑定 Base ID/IP，迁移/升级
// 换归属后下拍即投新家）。
//
// journal 文件 = 唯一权威，文本一行一记录（7 字段统一格式）：
//   state|order_id|player_id|received_ms|delivered_ms|base_generation|payload
//   state ∈ {P=待办, D=已投递}（P 行 delivered_ms/base_generation 为 0）；
//   同单后行覆盖前行（append-only 事件序）。open() 回放重建索引后做快照
//   压薄：每单一行 tmp+rename 重写（崩溃留旧文件，事件序仍完整可回放）。
// payload 禁含 '\n' 与 '|'（行格式约束；渠道载荷须单行无管道符——accept
// 显式拒绝，不静默截断）。
//
// 线程模型：类内互斥（受理面/投递面两线程安全）；find() 返回内部指针，
// 假定读取与写操作不同时（与 PlayerDirectory::find 同口径）。

#include <cstdint>
#include <fstream>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace apollo::game::orders {

struct OrderRecord {
    std::uint64_t order_id = 0;      // 渠道单号（幂等键）
    std::uint64_t player_id = 0;     // 稳定玩家身份（投递解析键）
    std::string payload;             // 渠道通知载荷（发货 executor 的输入）
    std::uint64_t receive_seq = 0;   // 受理序（内部单调；待办稳定排序）
    std::int64_t received_ms = 0;
    std::int64_t delivered_ms = 0;   // 0 = 未完成投递
    std::uint64_t delivered_base_generation = 0;  // 完成投递时的 Base 归属代次

    [[nodiscard]] bool delivered() const noexcept { return delivered_ms != 0; }
};

class OrderStore {
public:
    struct AcceptResult {
        bool accepted{false};   // 本次新建且已持久化（可回执「已收到」）
        bool duplicate{false};  // 已知单号幂等命中（渠道重发，非错误；不重建不重置进度）
    };

    explicit OrderStore(std::string journal_path);

    // 打开/恢复：文件存在则回放重建索引 + 快照压薄；目录缺失/不可写 →
    // false（调用方显形——待办清单不允许静默丢失）。
    bool open();

    // 可靠受理（幂等键 order_id）：append+flush 成功才算 accepted。单号
    // 0/玩家 0/payload 含 '\n' 或 '|' → {false,false}（行格式/身份面拒绝）。
    AcceptResult accept(std::uint64_t order_id, std::uint64_t player_id,
                        const std::string& payload, std::int64_t now_ms);

    // 完成投递（append+flush 持久置墓碑）。未知单号/已投递/归属代次 0
    // （解析缝必须给出有效代次才算完成）→ false。写盘失败回滚内存不冒充
    // 完成（订单留在待办，下拍重投）。
    bool mark_delivered(std::uint64_t order_id, std::uint64_t base_generation,
                        std::int64_t now_ms);

    // 待办快照（受理序稳定排序；投递驱动的消费面）。
    std::vector<OrderRecord> pending() const;

    const OrderRecord* find(std::uint64_t order_id) const;
    std::size_t pending_count() const;
    std::size_t delivered_count() const;
    std::size_t total_count() const;

private:
    std::string render_line(const OrderRecord& record) const;
    bool append_line(const std::string& line);
    bool replay();
    void compact_snapshot();

    mutable std::mutex mutex_;
    std::string journal_path_;
    std::map<std::uint64_t, OrderRecord> orders_;  // keyed by order_id
    std::uint64_t next_receive_seq_ = 1;
    std::ofstream out_;  // 常驻追加句柄（open 后；写失败关闭，下拍重开自愈）
    bool opened_ = false;
};

}  // namespace apollo::game::orders
