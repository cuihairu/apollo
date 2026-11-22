#pragma once

#include <cstdint>
#include <string>

namespace gateway {

// 本进程 M1 握手身份（本网关各主动/被动 Link 的 self_name 同值）。多网关同面
// 共存时，对端被动面按拨入方 self_name 建 peer 键（link_manager 按名消歧）——
// 各网关必须以进程唯一身份拨入，同号互顶（D10 case 1 实测：第二网关顶掉首
// 网关 peer 槽致其准入 RPC 失败）。gateway_id 未配（0）= 单网关/测试缺省，
// 回退旧值 "gateway"（兼容既有装配与测试）。
inline std::string gateway_self_name(std::uint32_t gateway_id) {
    if (gateway_id == 0) {
        return std::string("gateway");
    }
    return "gateway-" + std::to_string(gateway_id);
}

// 会话/请求号基址（多网关身份域）：进程内单调计数器以基址起。单网关
// （gateway_id 0）基址 1（兼容旧编号）；多网关以 gateway_id 派生高位域承载
// 进程身份——会话/请求号跨网关唯一（两网关各自从 1 编号会在 World 会话表 /
// 请求关联键上互撞，破「两会话各自有效互不覆盖」）。返回恒非 0（0 = 未配
// 发哨兵）；gateway_id 高位 32 位、进程内计数器低位 32 位（4e9 会话/网关够用）。
inline std::uint64_t gateway_id_base(std::uint32_t gateway_id) {
    if (gateway_id == 0) {
        return 1;
    }
    return (static_cast<std::uint64_t>(gateway_id) << 32) | 1u;
}

} // namespace gateway
