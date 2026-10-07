#pragma once

// 在线目录跨进程镜像（G-1 收尾批增量②；session-and-online-directory §5/§7/§8
// 的 G-1 平面骨架）。
//
// 设计口径（§7 进程间共享模型）：目录 = manager 域进程内存权威态，跨进程
// 可见性 = 「owner 广播 delta，消费者本地镜像 + seq 续传」——本文件给该
// 模型的协议与两侧行为件，传输字节面注入（send 函数 / on_datagram 收包），
// 不含 socket（UDP datagram 机制归 net/discovery，app 层接线）。
//
//   - DirectoryPublisher  owner 侧：PlayerDirectory::Event → 增量 wire
//     （seq 单调）+ 全量快照（分片 SnapshotReply，周期对账/失配 healing）；
//   - DirectoryMirror     消费侧：只读投影（最终一致，秒级）——seq 连续
//     应用 / 重复丢 / 断档 stale + 快照请求钩子 / SnapshotReply 分片重组
//     重置；查询面 find/online_ids/size。
//
// 一致性声明（§5）：镜像 = 投影，裁决与权威读只在 manager——读旧镜像的
// 最坏后果 = 一次失败调用 + re-resolve，不破坏单写者。
//
// wire 格式（"APD2" 家族，与 G-1 发现面 "APD1" 32B 定长报文分立——目录
// delta/快照载荷超 32B，单包变长、定长字段逐字段小端编解码不 memcpy 结构
// 体；gateway_addr 不上 wire——镜像是定位面不是连接面）：
//   公共头 8B：magic u32 | version u16 | kind u8 | reserved u8
//   Delta（kind 1）：seq u32 | event_kind u8 | reason u32 | 玩家条目 69B
//   SnapshotRequest（kind 2）：公共头即全包（8B——请求无载荷，应答走广播）
//   SnapshotReply（kind 3）：snapshot_id u32 | total_parts u16 | part_index
//     u16 | baseline_seq u32 | entry_count u16 | entry[entry_count]×69B
//   条目 69B：player_id u64 | anchor_epoch u64 | session_id u64 |
//     gateway_id u32 | zone_id u32 | state u8 | world_id u32 | map_id u64 |
//     instance_id u64 | space_id u64 | route_version u64
//
// 并发纪律：Publisher/Mirror 全部方法调用方单线程驱动（与 discovery 库同
// 源）；不起内部线程、不依赖钟——节奏由调用方注入。

#include "apollo/game/session/player_directory.hpp"
#include "apollo/game/session/world_assignment.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

namespace apollo::game::session {

// ---- wire 常量 ----

inline constexpr std::uint32_t kDirectoryWireMagic = 0x41504432u;  // "APD2"
inline constexpr std::uint16_t kDirectoryWireVersion = 1;
inline constexpr std::size_t kDirectoryWireHeaderSize = 8;
inline constexpr std::size_t kDirectoryEntryWireSize = 69;

enum class DirectoryWireKind : std::uint8_t {
    Delta = 1,
    SnapshotRequest = 2,
    SnapshotReply = 3,
};

// 镜像条目（消费侧视图——目录条目的定位面投影，无 gateway_addr）
struct MirrorEntry {
    std::uint64_t player_id = 0;
    std::uint64_t anchor_epoch = 0;
    std::uint64_t session_id = 0;
    std::uint32_t gateway_id = 0;
    std::uint32_t zone_id = 0;
    PlayerDirectory::EntryState state = PlayerDirectory::EntryState::Online;
    WorldAssignment assignment{};

    [[nodiscard]] bool operator==(const MirrorEntry&) const noexcept = default;
};

// ---- 编解码（返回 false = 载荷非法/越界，调用方整包丢）----

// Delta 条目区（event_kind + reason + 69B 条目字段；供 Delta 与测试复用）
struct DirectoryDeltaBody {
    std::uint32_t seq = 0;  // owner 单调序（镜像续传判据）
    PlayerDirectory::Event::Kind event_kind = PlayerDirectory::Event::Kind::SessionUp;
    std::uint32_t reason = 0;
    MirrorEntry entry{};
};

[[nodiscard]] std::size_t encode_delta(const DirectoryDeltaBody& body,
                                       std::uint8_t* buf, std::size_t cap);
[[nodiscard]] bool decode_delta(const std::uint8_t* data, std::size_t len,
                                DirectoryDeltaBody& out);

[[nodiscard]] std::size_t encode_snapshot_request(std::uint8_t* buf,
                                                  std::size_t cap);
[[nodiscard]] bool is_snapshot_request(const std::uint8_t* data, std::size_t len);

struct DirectorySnapshotPart {
    std::uint32_t snapshot_id = 0;
    std::uint16_t total_parts = 1;
    std::uint16_t part_index = 0;  // 从 0 起
    std::uint32_t baseline_seq = 0;
    std::vector<MirrorEntry> entries;
};

[[nodiscard]] std::size_t encode_snapshot_part(const DirectorySnapshotPart& part,
                                               std::uint8_t* buf, std::size_t cap);
[[nodiscard]] bool decode_snapshot_part(const std::uint8_t* data, std::size_t len,
                                        DirectorySnapshotPart& out);

// 单 part 条目上限（~9KB datagram，UDP 用户态安全余量内）
inline constexpr std::size_t kMaxSnapshotPartEntries = 128;

// ---- DirectoryPublisher：owner 侧发布件 ----

class DirectoryPublisher {
public:
    // 发包口注入（app 层接 UDP sendto；测试注入捕获桩）。false = 发送失败
    // （发布件不重试——UDP 丢失由快照周期 healing，§2 事件丢不兜底）
    using SendFn = std::function<bool(const std::uint8_t* data, std::size_t len)>;

    explicit DirectoryPublisher(SendFn send);

    // 目录事件入口（app 层从 PlayerDirectory::EventSink 链入）：seq 自增 +
    // Delta 编码发包。player_id 0 事件拒发（目录无零号玩家条目）。
    void publish(const PlayerDirectory::Event& event);

    // 全量快照发布：条目分片（每片 ≤ kMaxSnapshotPartEntries）连发，新
    // snapshot_id + baseline_seq = 当前 seq（镜像重组后以快照为权威态续传）。
    // 返回发送分片数（目录空表 = 1 片空快照，同样发布）。
    std::size_t publish_snapshot(const std::vector<MirrorEntry>& authoritative);

    [[nodiscard]] std::uint32_t seq() const noexcept {
        return seq_;
    }

private:
    SendFn send_;
    std::uint32_t seq_ = 0;      // Delta 单调序（镜像续传判据）
    std::uint32_t next_snapshot_id_ = 1;
};

// ---- DirectoryMirror：消费侧只读投影 ----

class DirectoryMirror {
public:
    // 断档请求钩子（app 层接 SnapshotRequest 发包；库内每次进入 stale 态
    // 触发一次——重复断档不重复请求，快照到货即复位）
    using SnapshotRequester = std::function<void()>;

    // 收包口：解码分发（Delta / SnapshotReply；SnapshotRequest 到消费侧
    // 无意义，静默丢）。非法/截断包整包丢。
    void on_datagram(const std::uint8_t* data, std::size_t len);

    // Delta 应用（on_datagram 内部路径；测试/进程内直驱复用）
    void apply_delta(const DirectoryDeltaBody& body);

    // SnapshotReply 分片应用（同上）
    void apply_snapshot_part(const DirectorySnapshotPart& part);

    void set_snapshot_requester(SnapshotRequester requester);

    // ---- 查询面（§5 只读投影）----

    const MirrorEntry* find(std::uint64_t player_id) const;
    std::vector<std::uint64_t> online_ids() const;
    [[nodiscard]] std::size_t size() const noexcept {
        return entries_.size();
    }
    // 断档缺照状态：true = seq 断档等快照（投影不可信面，消费方自守——
    // 路由失败走 re-resolve，§5 一致性声明）
    [[nodiscard]] bool stale() const noexcept {
        return stale_;
    }
    [[nodiscard]] std::uint32_t last_seq() const noexcept {
        return last_seq_;
    }

private:
    void enter_stale();

    std::unordered_map<std::uint64_t, MirrorEntry> entries_;
    std::uint32_t last_seq_ = 0;      // 已应用最大 Delta seq
    bool has_seq_ = false;            // 尚未收到任何 Delta
    bool stale_ = false;              // 断档缺照（等快照重置）
    bool request_inflight_ = false;   // 本轮断档已发请求（到货复位）
    SnapshotRequester requester_;
    // 快照重组态（跨包：分片严格按 part_index 升序收，乱序/换 id 即重启）
    std::uint32_t assembling_snapshot_id_ = 0;
    std::uint16_t assembled_parts_ = 0;
    std::uint16_t next_part_index_ = 0;
    std::vector<MirrorEntry> assembled_entries_;
};

} // namespace apollo::game::session
