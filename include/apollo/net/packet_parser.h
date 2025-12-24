#pragma once

#include <cstdint>
#include <functional>
#include <memory>

namespace apollo {
namespace net {

/**
 * @brief 数据包解析结果
 */
struct ParseResult {
    bool isValid;           ///< 是否为有效数据包
    const char* data;       ///< 数据包起始位置
    uint32_t length;        ///< 数据包长度

    /**
     * @brief 创建有效的解析结果
     */
    static ParseResult valid(const char* d, uint32_t len) {
        return {true, d, len};
    }

    /**
     * @brief 创建无效结果(需要更多数据)
     */
    static ParseResult invalid() {
        return {false, nullptr, 0};
    }

    /**
     * @brief 数据不足需要更多数据
     */
    static ParseResult needMore() {
        return {false, nullptr, 0};
    }
};

/**
 * @brief 数据包处理回调
 *
 * @param sessionId 会话ID
 * @param packet 数据包起始位置
 * @param length 数据包长度
 */
using PacketHandler = std::function<void(uint64_t sessionId, const char* packet, uint32_t length)>;

/**
 * @brief 抽象数据包解析器接口
 *
 * 负责从二进制流中解析出完整的数据包
 * 支持粘包/半包处理
 */
class PacketParser {
public:
    virtual ~PacketParser() = default;

    /**
     * @brief 解析数据包
     * @param data 接收到的数据缓冲区
     * @param length 缓冲区长度
     * @return 解析结果
     *
     * @note 如果返回无效结果，表示需要更多数据
     *       网络层会继续积累数据直到能解析出完整包
     */
    virtual ParseResult parse(const char* data, uint32_t length) = 0;

    /**
     * @brief 重置解析器状态
     */
    virtual void reset() = 0;

    /**
     * @brief 获取最大数据包大小
     */
    virtual uint32_t getMaxPacketSize() const = 0;

    /**
     * @brief 获取数据包头大小
     */
    virtual uint32_t getHeaderSize() const = 0;

    /**
     * @brief 设置数据包处理器
     */
    virtual void setPacketHandler(PacketHandler handler) = 0;
};

/**
 * @brief 数据包解析器智能指针类型
 */
using PacketParserPtr = std::shared_ptr<PacketParser>;

} // namespace net
} // namespace apollo
