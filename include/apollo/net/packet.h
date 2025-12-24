#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <unordered_map>

namespace apollo {
namespace net {

//==============================================================================
// 数据包缓冲区 - 高效的包构建
//==============================================================================

class PacketBuffer {
public:
    PacketBuffer() = default;
    explicit PacketBuffer(size_t capacity) { buffer_.reserve(capacity); }

    // 写入原始字节
    void writeBytes(const void* data, size_t length) {
        const char* ptr = static_cast<const char*>(data);
        buffer_.insert(buffer_.end(), ptr, ptr + length);
    }

    // 写入整数（小端序）
    void writeUInt8(uint8_t value) {
        buffer_.push_back(value);
    }

    void writeUInt16LE(uint16_t value) {
        buffer_.push_back(value & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
    }

    void writeUInt32LE(uint32_t value) {
        buffer_.push_back(value & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
        buffer_.push_back((value >> 16) & 0xFF);
        buffer_.push_back((value >> 24) & 0xFF);
    }

    void writeUInt64LE(uint64_t value) {
        writeUInt32LE(value & 0xFFFFFFFF);
        writeUInt32LE((value >> 32) & 0xFFFFFFFF);
    }

    // 写入整数（大端序/网络字节序）
    void writeUInt16BE(uint16_t value) {
        buffer_.push_back((value >> 8) & 0xFF);
        buffer_.push_back(value & 0xFF);
    }

    void writeUInt32BE(uint32_t value) {
        buffer_.push_back((value >> 24) & 0xFF);
        buffer_.push_back((value >> 16) & 0xFF);
        buffer_.push_back((value >> 8) & 0xFF);
        buffer_.push_back(value & 0xFF);
    }

    void writeUInt64BE(uint64_t value) {
        writeUInt32BE((value >> 32) & 0xFFFFFFFF);
        writeUInt32BE(value & 0xFFFFFFFF);
    }

    // 写入字符串
    void writeString(const std::string& str) {
        writeBytes(str.data(), str.size());
    }

    // 写入带长度前缀的字符串
    void writeLPString(const std::string& str, bool length16 = false) {
        if (length16) {
            writeUInt16LE(static_cast<uint16_t>(str.size()));
        } else {
            writeUInt8(static_cast<uint8_t>(str.size()));
        }
        writeString(str);
    }

    // 写入定长字符串
    void writeFixedString(const std::string& str, size_t length) {
        size_t writeLen = str.size() < length ? str.size() : length;
        writeBytes(str.data(), writeLen);
        // 填充零
        for (size_t i = writeLen; i < length; ++i) {
            buffer_.push_back(0);
        }
    }

    // 填充
    void pad(size_t count, uint8_t value = 0) {
        buffer_.insert(buffer_.end(), count, value);
    }

    // 预留空间
    void reserve(size_t capacity) {
        buffer_.reserve(capacity);
    }

    // 获取缓冲区
    const std::vector<char>& buffer() const { return buffer_; }
    std::vector<char>& buffer() { return buffer_; }

    // 获取数据指针
    const char* data() const { return buffer_.data(); }
    char* data() { return buffer_.data(); }

    // 大小
    size_t size() const { return buffer_.size(); }
    bool empty() const { return buffer_.empty(); }

    // 清空
    void clear() { buffer_.clear(); }

    // 交换
    void swap(PacketBuffer& other) noexcept {
        buffer_.swap(other.buffer_);
    }

    // 释放所有权
    std::vector<char> release() {
        return std::move(buffer_);
    }

private:
    std::vector<char> buffer_;
};

//==============================================================================
// 数据包读取器
//==============================================================================

class PacketReader {
public:
    PacketReader() = default;
    PacketReader(const void* data, size_t size)
        : data_(static_cast<const char*>(data)), size_(size) {}

    PacketReader(const std::vector<char>& buffer)
        : data_(buffer.data()), size_(buffer.size()) {}

    void setData(const void* data, size_t size) {
        data_ = static_cast<const char*>(data);
        size_ = size;
        pos_ = 0;
    }

    // 读取原始字节
    bool readBytes(void* buffer, size_t length) {
        if (pos_ + length > size_) return false;
        std::memcpy(buffer, data_ + pos_, length);
        pos_ += length;
        return true;
    }

    // 预览数据（不移动指针）
    const char* peek(size_t offset = 0) const {
        if (pos_ + offset >= size_) return nullptr;
        return data_ + pos_ + offset;
    }

    // 读取整数（小端序）
    bool readUInt8(uint8_t& value) {
        if (pos_ >= size_) return false;
        value = static_cast<uint8_t>(data_[pos_++]);
        return true;
    }

    bool readUInt16LE(uint16_t& value) {
        if (pos_ + 2 > size_) return false;
        value = static_cast<uint8_t>(data_[pos_]) |
                (static_cast<uint8_t>(data_[pos_ + 1]) << 8);
        pos_ += 2;
        return true;
    }

    bool readUInt32LE(uint32_t& value) {
        if (pos_ + 4 > size_) return false;
        value = static_cast<uint8_t>(data_[pos_]) |
               (static_cast<uint8_t>(data_[pos_ + 1]) << 8) |
               (static_cast<uint8_t>(data_[pos_ + 2]) << 16) |
               (static_cast<uint8_t>(data_[pos_ + 3]) << 24);
        pos_ += 4;
        return true;
    }

    bool readUInt64LE(uint64_t& value) {
        uint32_t low, high;
        if (!readUInt32LE(low)) return false;
        if (!readUInt32LE(high)) return false;
        value = (static_cast<uint64_t>(high) << 32) | low;
        return true;
    }

    // 读取整数（大端序）
    bool readUInt16BE(uint16_t& value) {
        if (pos_ + 2 > size_) return false;
        value = (static_cast<uint8_t>(data_[pos_]) << 8) |
                static_cast<uint8_t>(data_[pos_ + 1]);
        pos_ += 2;
        return true;
    }

    bool readUInt32BE(uint32_t& value) {
        if (pos_ + 4 > size_) return false;
        value = (static_cast<uint8_t>(data_[pos_]) << 24) |
               (static_cast<uint8_t>(data_[pos_ + 1]) << 16) |
               (static_cast<uint8_t>(data_[pos_ + 2]) << 8) |
               static_cast<uint8_t>(data_[pos_ + 3]);
        pos_ += 4;
        return true;
    }

    // 读取字符串
    bool readString(std::string& str, size_t length) {
        if (pos_ + length > size_) return false;
        str.assign(data_ + pos_, length);
        pos_ += length;
        return true;
    }

    // 读取带长度前缀的字符串
    bool readLPString(std::string& str, bool length16 = false) {
        uint16_t len16;
        uint8_t len8;
        size_t len;

        if (length16) {
            if (!readUInt16LE(len16)) return false;
            len = len16;
        } else {
            if (!readUInt8(len8)) return false;
            len = len8;
        }

        return readString(str, len);
    }

    // 跳过字节
    bool skip(size_t count) {
        if (pos_ + count > size_) return false;
        pos_ += count;
        return true;
    }

    // 定位
    bool seek(size_t pos) {
        if (pos > size_) return false;
        pos_ = pos;
        return true;
    }

    // 位置
    size_t position() const { return pos_; }

    // 剩余
    size_t remaining() const { return size_ - pos_; }
    bool hasRemaining() const { return pos_ < size_; }
    bool eos() const { return pos_ >= size_; }

    // 获取原始数据
    const char* data() const { return data_; }
    size_t size() const { return size_; }

private:
    const char* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

//==============================================================================
// 分片组装器（处理大包分片）
//==============================================================================

class PacketAssembler {
public:
    static constexpr size_t MAX_PACKET_SIZE = 64 * 1024; // 64KB
    static constexpr size_t DEFAULT_FRAGMENT_SIZE = 1400; // MTU

    struct Fragment {
        uint16_t packetId;
        uint8_t fragmentIndex;
        uint8_t totalFragments;
        std::vector<char> data;
    };

    explicit PacketAssembler(size_t maxSize = MAX_PACKET_SIZE)
        : maxSize_(maxSize) {}

    // 分片数据包
    std::vector<Fragment> fragment(const void* data, size_t size,
                                   uint16_t packetId, size_t fragmentSize = DEFAULT_FRAGMENT_SIZE) {
        std::vector<Fragment> fragments;

        if (size > maxSize_) {
            return fragments; // 包太大
        }

        const char* ptr = static_cast<const char*>(data);
        size_t offset = 0;
        uint8_t index = 0;
        uint8_t total = static_cast<uint8_t>((size + fragmentSize - 1) / fragmentSize);

        while (offset < size) {
            Fragment frag;
            frag.packetId = packetId;
            frag.fragmentIndex = index++;
            frag.totalFragments = total;

            size_t chunkSize = std::min(fragmentSize, size - offset);
            frag.data.assign(ptr + offset, ptr + offset + chunkSize);

            fragments.push_back(std::move(frag));
            offset += chunkSize;
        }

        return fragments;
    }

    // 组装分片
    struct AssemblyResult {
        bool complete = false;
        uint16_t packetId = 0;
        std::vector<char> data;
    };

    AssemblyResult assemble(const Fragment& fragment) {
        AssemblyResult result;
        result.packetId = fragment.packetId;

        auto& state = fragments_[fragment.packetId];

        // 初始化
        if (state.totalFragments == 0) {
            state.totalFragments = fragment.totalFragments;
            state.receivedFragments.resize(fragment.totalFragments, false);
            state.data.resize(fragment.totalFragments);
        }

        // 验证
        if (fragment.fragmentIndex >= state.totalFragments) {
            return result;
        }

        // 存储分片
        state.data[fragment.fragmentIndex] = fragment.data;
        state.receivedFragments[fragment.fragmentIndex] = true;
        ++state.receivedCount;

        // 检查是否完整
        if (state.receivedCount == state.totalFragments) {
            result.complete = true;

            // 计算总大小
            size_t totalSize = 0;
            for (const auto& frag : state.data) {
                totalSize += frag.size();
            }

            // 合并数据
            result.data.reserve(totalSize);
            for (const auto& frag : state.data) {
                result.data.insert(result.data.end(), frag.begin(), frag.end());
            }

            // 清理
            fragments_.erase(fragment.packetId);
        }

        return result;
    }

    // 超时清理
    void cleanup() {
        // TODO: 添加时间戳，清理超时的分片
    }

    // 取消某个包的组装
    bool cancel(uint16_t packetId) {
        return fragments_.erase(packetId) > 0;
    }

private:
    struct AssemblyState {
        uint8_t totalFragments = 0;
        uint8_t receivedCount = 0;
        std::vector<bool> receivedFragments;
        std::vector<std::vector<char>> data;
    };

    size_t maxSize_;
    std::unordered_map<uint16_t, AssemblyState> fragments_;
};

//==============================================================================
// 数据包校验
//==============================================================================

class PacketChecksum {
public:
    // 简单异或校验
    static uint8_t xor8(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint8_t checksum = 0;
        for (size_t i = 0; i < size; ++i) {
            checksum ^= ptr[i];
        }
        return checksum;
    }

    // 16位异或校验
    static uint16_t xor16(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint16_t checksum = 0;
        for (size_t i = 0; i + 1 < size; i += 2) {
            checksum ^= (static_cast<uint16_t>(ptr[i]) << 8) | ptr[i + 1];
        }
        if (size % 2 == 1) {
            checksum ^= static_cast<uint16_t>(ptr[size - 1]) << 8;
        }
        return checksum;
    }

    // 校验和（Internet Checksum）
    static uint16_t internetChecksum(const void* data, size_t size) {
        const uint16_t* ptr = static_cast<const uint16_t*>(data);
        size_t count = size / 2;

        uint32_t sum = 0;
        for (size_t i = 0; i < count; ++i) {
            sum += ptr[i];
        }

        // 处理奇数长度
        if (size % 2 == 1) {
            sum += static_cast<const uint8_t*>(data)[size - 1];
        }

        // 折叠
        while (sum >> 16) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }

        return ~sum;
    }

    // CRC-8
    static uint8_t crc8(const void* data, size_t size, uint8_t poly = 0x07) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint8_t crc = 0;

        for (size_t i = 0; i < size; ++i) {
            crc ^= ptr[i];
            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 0x80) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
            }
        }

        return crc;
    }

    // CRC-16 (CCITT)
    static uint16_t crc16(const void* data, size_t size, uint16_t poly = 0x1021) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint16_t crc = 0;

        for (size_t i = 0; i < size; ++i) {
            crc ^= (static_cast<uint16_t>(ptr[i]) << 8);
            for (int bit = 0; bit < 8; ++bit) {
                if (crc & 0x8000) {
                    crc = (crc << 1) ^ poly;
                } else {
                    crc <<= 1;
                }
            }
        }

        return crc;
    }

    // CRC-32
    static uint32_t crc32(const void* data, size_t size) {
        static const uint32_t table[256] = {
            0x00000000, 0x77073096, 0xee0e612c, 0x990951ba,
            0x076dc419, 0x706af48f, 0xe963a535, 0x9e6495a3,
            // ... (简化版，实际使用完整表)
        };

        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint32_t crc = 0xFFFFFFFF;

        for (size_t i = 0; i < size; ++i) {
            uint8_t index = (crc ^ ptr[i]) & 0xFF;
            crc = (crc >> 8) ^ table[index];
        }

        return crc ^ 0xFFFFFFFF;
    }

    // FNV-1a Hash
    static uint32_t fnv1a32(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        const uint32_t prime = 16777619;
        uint32_t hash = 2166136261;

        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= prime;
        }

        return hash;
    }

    static uint64_t fnv1a64(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        const uint64_t prime = 1099511628211ULL;
        uint64_t hash = 14695981039346656037ULL;

        for (size_t i = 0; i < size; ++i) {
            hash ^= ptr[i];
            hash *= prime;
        }

        return hash;
    }
};

//==============================================================================
// 数据包头定义
//==============================================================================

#pragma pack(push, 1)

struct PacketHeader {
    uint16_t packetId;      // 包ID
    uint16_t length;        // 数据长度
    uint32_t sequence;      // 序列号
    uint8_t flags;          // 标志位
    uint8_t checksum;       // 校验和

    // 标志位定义
    static constexpr uint8_t FLAG_FRAGMENTED = 0x01;   // 分片包
    static constexpr uint8_t FLAG_COMPRESSED = 0x02;   // 压缩包
    static constexpr uint8_t FLAG_ENCRYPTED = 0x04;    // 加密包
    static constexpr uint8_t FLAG_RELIABLE = 0x08;     // 可靠传输

    bool isFragmented() const { return (flags & FLAG_FRAGMENTED) != 0; }
    bool isCompressed() const { return (flags & FLAG_COMPRESSED) != 0; }
    bool isEncrypted() const { return (flags & FLAG_ENCRYPTED) != 0; }
    bool isReliable() const { return (flags & FLAG_RELIABLE) != 0; }
};

struct FragmentHeader {
    uint16_t packetId;      // 原包ID
    uint8_t fragmentIndex;  // 当前分片索引
    uint8_t totalFragments; // 总分片数
    uint16_t offset;        // 数据偏移
    uint16_t length;        // 分片数据长度
};

#pragma pack(pop)

//==============================================================================
// MTU 工具
//==============================================================================

class MTU {
public:
    // 常见 MTU 值
    static constexpr int MIN_MTU = 576;          // 最小 MTU
    static constexpr int ETHERNET_MTU = 1500;    // 以太网 MTU
    static constexpr int JUMBO_FRAME = 9000;     // 巨型帧
    static constexpr int LOOPBACK_MTU = 65536;   // 回环 MTU

    // 计算安全的数据包大小（考虑 IP + UDP 头）
    static constexpr int safeUdpSize(int mtu = ETHERNET_MTU) {
        return mtu - 20 - 8; // IP(20) + UDP(8)
    }

    // 计算 TCP 的安全数据包大小
    static constexpr int safeTcpSize(int mtu = ETHERNET_MTU) {
        return mtu - 20 - 20; // IP(20) + TCP(20)
    }

    // 估算路径 MTU（简化版）
    static int estimatePathMTU(const std::string& host) {
        // TODO: 实现 PMTU 发现
        return ETHERNET_MTU;
    }
};

} // namespace net
} // namespace apollo
