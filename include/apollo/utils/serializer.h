#pragma once

#include <vector>
#include <string>
#include <cstring>
#include <cstdint>
#include <type_traits>
#include <map>
#include <unordered_map>

namespace apollo {
namespace utils {

//==============================================================================
// 字节序转换
//==============================================================================

inline bool isLittleEndian() {
    uint16_t test = 0x0001;
    return *reinterpret_cast<uint8_t*>(&test) == 0x01;
}

inline uint16_t swap16(uint16_t value) {
    return ((value & 0xFF) << 8) | ((value >> 8) & 0xFF);
}

inline uint32_t swap32(uint32_t value) {
    return ((value & 0xFF) << 24) |
           ((value & 0xFF00) << 8) |
           ((value >> 8) & 0xFF00) |
           ((value >> 24) & 0xFF);
}

inline uint64_t swap64(uint64_t value) {
    return ((value & 0xFFULL) << 56) |
           ((value & 0xFF00ULL) << 40) |
           ((value & 0xFF0000ULL) << 24) |
           ((value & 0xFF000000ULL) << 8) |
           ((value >> 8) & 0xFF000000ULL) |
           ((value >> 16) & 0xFF0000ULL) |
           ((value >> 24) & 0xFF00ULL) |
           ((value >> 56) & 0xFFULL);
}

template<typename T>
inline T hostToNetwork(T value) {
    if constexpr (sizeof(T) == 2) {
        return isLittleEndian() ? swap16(static_cast<uint16_t>(value)) : value;
    } else if constexpr (sizeof(T) == 4) {
        return isLittleEndian() ? swap32(static_cast<uint32_t>(value)) : value;
    } else if constexpr (sizeof(T) == 8) {
        return isLittleEndian() ? swap64(static_cast<uint64_t>(value)) : value;
    }
    return value;
}

template<typename T>
inline T networkToHost(T value) {
    return hostToNetwork(value);
}

//==============================================================================
// BinaryWriter - 二进制写入器
//==============================================================================

class BinaryWriter {
public:
    BinaryWriter() = default;
    explicit BinaryWriter(size_t reserved) {
        buffer_.reserve(reserved);
    }

    // 写入基础类型
    template<typename T>
    void write(T value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        T netValue = hostToNetwork(value);
        const char* data = reinterpret_cast<const char*>(&netValue);
        buffer_.insert(buffer_.end(), data, data + sizeof(T));
    }

    // 写入字节数组
    void writeBytes(const void* data, size_t length) {
        const char* ptr = static_cast<const char*>(data);
        buffer_.insert(buffer_.end(), ptr, ptr + length);
    }

    // 写入字符串（带长度前缀）
    void writeString(const std::string& str) {
        write(static_cast<uint32_t>(str.size()));
        writeBytes(str.data(), str.size());
    }

    // 写入定长字符串
    void writeFixedString(const std::string& str, size_t maxLength) {
        size_t len = std::min(str.size(), maxLength);
        writeBytes(str.data(), len);
        if (len < maxLength) {
            buffer_.insert(buffer_.end(), maxLength - len, '\0');
        }
    }

    // 写入布尔值
    void writeBool(bool value) {
        buffer_.push_back(value ? 1 : 0);
    }

    // 写入浮点数
    void writeFloat(float value) {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        write(bits);
    }

    void writeDouble(double value) {
        uint64_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        write(bits);
    }

    // 写入容器
    template<typename T>
    void writeVector(const std::vector<T>& vec) {
        write(static_cast<uint32_t>(vec.size()));
        for (const auto& item : vec) {
            write(item);
        }
    }

    // 模板递归终止条件
    void write() {}

    // 可变参数写入
    template<typename T, typename... Args>
    void write(T first, Args... args) {
        write(first);
        write(args...);
    }

    // 获取缓冲区
    const std::vector<char>& buffer() const { return buffer_; }
    std::vector<char>& buffer() { return buffer_; }

    // 清空
    void clear() { buffer_.clear(); }

    // 获取大小
    size_t size() const { return buffer_.size(); }

    // 获取数据指针
    const char* data() const { return buffer_.data(); }

private:
    std::vector<char> buffer_;
};

//==============================================================================
// BinaryReader - 二进制读取器
//==============================================================================

class BinaryReader {
public:
    BinaryReader() = default;
    BinaryReader(const void* data, size_t size)
        : data_(static_cast<const char*>(data)), size_(size) {}

    BinaryReader(const std::vector<char>& buffer)
        : data_(buffer.data()), size_(buffer.size()) {}

    // 设置数据
    void setData(const void* data, size_t size) {
        data_ = static_cast<const char*>(data);
        size_ = size;
        pos_ = 0;
    }

    // 读取基础类型
    template<typename T>
    bool read(T& value) {
        static_assert(std::is_trivially_copyable_v<T>, "Type must be trivially copyable");
        if (pos_ + sizeof(T) > size_) {
            return false;
        }
        T netValue;
        std::memcpy(&netValue, data_ + pos_, sizeof(T));
        value = networkToHost(netValue);
        pos_ += sizeof(T);
        return true;
    }

    // 读取字节数组
    bool readBytes(void* buffer, size_t length) {
        if (pos_ + length > size_) {
            return false;
        }
        std::memcpy(buffer, data_ + pos_, length);
        pos_ += length;
        return true;
    }

    // 读取字符串
    bool readString(std::string& str) {
        uint32_t len;
        if (!read(len)) {
            return false;
        }
        if (pos_ + len > size_) {
            return false;
        }
        str.assign(data_ + pos_, len);
        pos_ += len;
        return true;
    }

    // 读取定长字符串
    bool readFixedString(std::string& str, size_t maxLength) {
        if (pos_ + maxLength > size_) {
            return false;
        }
        size_t len = std::strlen(data_ + pos_);
        if (len > maxLength) {
            len = maxLength;
        }
        str.assign(data_ + pos_, len);
        pos_ += maxLength;
        return true;
    }

    // 读取布尔值
    bool readBool(bool& value) {
        if (pos_ >= size_) {
            return false;
        }
        value = data_[pos_++] != 0;
        return true;
    }

    // 读取浮点数
    bool readFloat(float& value) {
        uint32_t bits;
        if (!read(bits)) {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(bits));
        return true;
    }

    bool readDouble(double& value) {
        uint64_t bits;
        if (!read(bits)) {
            return false;
        }
        std::memcpy(&value, &bits, sizeof(bits));
        return true;
    }

    // 读取容器
    template<typename T>
    bool readVector(std::vector<T>& vec) {
        uint32_t size;
        if (!read(size)) {
            return false;
        }
        vec.resize(size);
        for (uint32_t i = 0; i < size; ++i) {
            if (!read(vec[i])) {
                return false;
            }
        }
        return true;
    }

    // 跳过字节
    bool skip(size_t bytes) {
        if (pos_ + bytes > size_) {
            return false;
        }
        pos_ += bytes;
        return true;
    }

    // 获取当前位置
    size_t position() const { return pos_; }

    // 设置位置
    bool seek(size_t pos) {
        if (pos > size_) {
            return false;
        }
        pos_ = pos;
        return true;
    }

    // 是否还有数据
    bool hasRemaining() const { return pos_ < size_; }

    // 剩余字节数
    size_t remaining() const { return size_ - pos_; }

    // 是否到达末尾
    bool eos() const { return pos_ >= size_; }

private:
    const char* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
};

//==============================================================================
// 协议编解码辅助类
//==============================================================================

class ProtocolCodec {
public:
    // 编码消息头
    static void encodeHeader(BinaryWriter& writer, uint16_t msgId, uint32_t seq = 0) {
        writer.write(msgId);
        writer.write(seq);
        writer.write(static_cast<uint32_t>(0)); // 占位：body长度
    }

    // 补充消息体长度
    static void fillBodyLength(std::vector<char>& buffer, size_t headerSize) {
        if (buffer.size() < headerSize + 4) {
            return;
        }
        uint32_t bodyLen = static_cast<uint32_t>(buffer.size() - headerSize);
        bodyLen = hostToNetwork(bodyLen);
        std::memcpy(buffer.data() + headerSize - 4, &bodyLen, 4);
    }

    // 解码消息头
    static bool decodeHeader(BinaryReader& reader, uint16_t& msgId, uint32_t& seq, uint32_t& bodyLen) {
        if (!reader.read(msgId)) return false;
        if (!reader.read(seq)) return false;
        if (!reader.read(bodyLen)) return false;
        return true;
    }

    // 计算校验和（简单异或校验）
    static uint8_t calculateChecksum(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint8_t checksum = 0;
        for (size_t i = 0; i < size; ++i) {
            checksum ^= ptr[i];
        }
        return checksum;
    }

    // CRC16 校验
    static uint16_t calculateCRC16(const void* data, size_t size) {
        const uint8_t* ptr = static_cast<const uint8_t*>(data);
        uint16_t crc = 0xFFFF;
        for (size_t i = 0; i < size; ++i) {
            crc ^= static_cast<uint16_t>(ptr[i]);
            for (int j = 0; j < 8; ++j) {
                if (crc & 0x0001) {
                    crc = (crc >> 1) ^ 0xA001;
                } else {
                    crc >>= 1;
                }
            }
        }
        return crc;
    }
};

//==============================================================================
// Varint 编码（变长整数编码）
//==============================================================================

class Varint {
public:
    // 编码 uint32_t
    static size_t encodeUInt32(uint32_t value, char* output) {
        char* p = output;
        while (value > 0x7F) {
            *p++ = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        *p++ = value;
        return p - output;
    }

    // 解码 uint32_t
    static bool decodeUInt32(const char* input, size_t size, uint32_t& value, size_t& bytesConsumed) {
        value = 0;
        bytesConsumed = 0;
        uint32_t shift = 0;

        for (size_t i = 0; i < size && i < 5; ++i) {
            uint8_t byte = static_cast<uint8_t>(input[i]);
            value |= static_cast<uint32_t>(byte & 0x7F) << shift;
            bytesConsumed++;

            if (!(byte & 0x80)) {
                return true;
            }
            shift += 7;
        }
        return false; // 无效的 varint
    }

    // 编码 uint64_t
    static size_t encodeUInt64(uint64_t value, char* output) {
        char* p = output;
        while (value > 0x7F) {
            *p++ = (value & 0x7F) | 0x80;
            value >>= 7;
        }
        *p++ = static_cast<char>(value);
        return p - output;
    }

    // 解码 uint64_t
    static bool decodeUInt64(const char* input, size_t size, uint64_t& value, size_t& bytesConsumed) {
        value = 0;
        bytesConsumed = 0;
        uint64_t shift = 0;

        for (size_t i = 0; i < size && i < 10; ++i) {
            uint8_t byte = static_cast<uint8_t>(input[i]);
            value |= static_cast<uint64_t>(byte & 0x7F) << shift;
            bytesConsumed++;

            if (!(byte & 0x80)) {
                return true;
            }
            shift += 7;
        }
        return false; // 无效的 varint
    }

    // 计算编码后的字节数
    static size_t encodedSizeUInt32(uint32_t value) {
        if (value < (1U << 7)) {
            return 1;
        } else if (value < (1U << 14)) {
            return 2;
        } else if (value < (1U << 21)) {
            return 3;
        } else if (value < (1U << 28)) {
            return 4;
        } else {
            return 5;
        }
    }

    static size_t encodedSizeUInt64(uint64_t value) {
        if (value < (1ULL << 7)) {
            return 1;
        } else if (value < (1ULL << 14)) {
            return 2;
        } else if (value < (1ULL << 21)) {
            return 3;
        } else if (value < (1ULL << 28)) {
            return 4;
        } else if (value < (1ULL << 35)) {
            return 5;
        } else if (value < (1ULL << 42)) {
            return 6;
        } else if (value < (1ULL << 49)) {
            return 7;
        } else if (value < (1ULL << 56)) {
            return 8;
        } else if (value < (1ULL << 63)) {
            return 9;
        } else {
            return 10;
        }
    }
};

//==============================================================================
// ZigZag 编码（有符号整数编码）
//==============================================================================

class ZigZag {
public:
    // 编码 int32_t
    static uint32_t encode32(int32_t value) {
        return (static_cast<uint32_t>(value) << 1) ^ (value >> 31);
    }

    // 解码 int32_t
    static int32_t decode32(uint32_t value) {
        return (value >> 1) ^ -static_cast<int32_t>(value & 1);
    }

    // 编码 int64_t
    static uint64_t encode64(int64_t value) {
        return (static_cast<uint64_t>(value) << 1) ^ (value >> 63);
    }

    // 解码 int64_t
    static int64_t decode64(uint64_t value) {
        return (value >> 1) ^ -static_cast<int64_t>(value & 1);
    }
};

//==============================================================================
// BitWriter - 位级写入器
//==============================================================================

class BitWriter {
public:
    BitWriter() = default;
    explicit BitWriter(size_t reserved) : buffer_(reserved) {}

    // 写入单个位
    void writeBit(bool bit) {
        if (bitCount_ == 8) {
            flush();
        }
        if (bit) {
            currentByte_ |= (1 << (7 - bitCount_));
        }
        bitCount_++;
    }

    // 写入多个位
    void writeBits(uint32_t value, int numBits) {
        for (int i = numBits - 1; i >= 0; --i) {
            writeBit((value >> i) & 1);
        }
    }

    // 写入变长整数
    void writeVarint(uint32_t value) {
        char buffer[5];
        size_t size = Varint::encodeUInt32(value, buffer);
        writeBytes(buffer, size);
    }

    // 写入字节数据（对齐到字节边界）
    void writeBytes(const void* data, size_t length) {
        align();
        const char* ptr = static_cast<const char*>(data);
        buffer_.insert(buffer_.end(), ptr, ptr + length);
    }

    // 写入字符串
    void writeString(const std::string& str) {
        align();
        BinaryWriter writer;
        writer.writeString(str);
        writeBytes(writer.data(), writer.size());
    }

    // 对齐到字节边界
    void align() {
        if (bitCount_ > 0) {
            buffer_.push_back(currentByte_);
            currentByte_ = 0;
            bitCount_ = 0;
        }
    }

    // 获取缓冲区
    const std::vector<char>& buffer() const {
        return buffer_;
    }

    std::vector<char>& buffer() {
        return buffer_;
    }

    void clear() {
        buffer_.clear();
        currentByte_ = 0;
        bitCount_ = 0;
    }

    size_t size() const {
        return buffer_.size();
    }

private:
    void flush() {
        buffer_.push_back(currentByte_);
        currentByte_ = 0;
        bitCount_ = 0;
    }

    std::vector<char> buffer_;
    char currentByte_ = 0;
    int bitCount_ = 0;
};

//==============================================================================
// BitReader - 位级读取器
//==============================================================================

class BitReader {
public:
    BitReader() = default;
    BitReader(const void* data, size_t size)
        : data_(static_cast<const char*>(data)), size_(size) {}

    void setData(const void* data, size_t size) {
        data_ = static_cast<const char*>(data);
        size_ = size;
        pos_ = 0;
        bitPos_ = 0;
    }

    // 读取单个位
    bool readBit(bool& bit) {
        if (pos_ >= size_) {
            return false;
        }
        bit = (data_[pos_] >> (7 - bitPos_)) & 1;
        bitPos_++;
        if (bitPos_ == 8) {
            pos_++;
            bitPos_ = 0;
        }
        return true;
    }

    // 读取多个位
    bool readBits(uint32_t& value, int numBits) {
        value = 0;
        for (int i = 0; i < numBits; ++i) {
            bool bit;
            if (!readBit(bit)) {
                return false;
            }
            value = (value << 1) | (bit ? 1 : 0);
        }
        return true;
    }

    // 读取变长整数
    bool readVarint(uint32_t& value) {
        align();
        size_t consumed;
        return Varint::decodeUInt32(data_ + pos_, size_ - pos_, value, consumed);
        pos_ += consumed;
    }

    // 读取字节数据（对齐到字节边界）
    bool readBytes(void* buffer, size_t length) {
        align();
        if (pos_ + length > size_) {
            return false;
        }
        std::memcpy(buffer, data_ + pos_, length);
        pos_ += length;
        return true;
    }

    // 对齐到字节边界
    void align() {
        if (bitPos_ > 0) {
            pos_++;
            bitPos_ = 0;
        }
    }

    bool hasRemaining() const { return pos_ < size_; }
    size_t remaining() const { return size_ - pos_; }

private:
    const char* data_ = nullptr;
    size_t size_ = 0;
    size_t pos_ = 0;
    int bitPos_ = 0;
};

//==============================================================================
// 辅助宏：定义可序列化结构体
//==============================================================================

#define SERIALIZE_BEGIN() \
    void serialize(apollo::utils::BinaryWriter& writer) const { \
        (void)writer;

#define SERIALIZE_FIELD(field) \
        writer.write(field);

#define SERIALIZE_FIELD_BYTES(field, len) \
        writer.writeBytes(field, len);

#define SERIALIZE_FIELD_STRING(field) \
        writer.writeString(field);

#define SERIALIZE_END() \
    }

#define DESERIALIZE_BEGIN() \
    bool deserialize(apollo::utils::BinaryReader& reader) { \
        (void)reader;

#define DESERIALIZE_FIELD(field) \
        if (!reader.read(field)) return false;

#define DESERIALIZE_FIELD_BYTES(field, len) \
        if (!reader.readBytes(field, len)) return false;

#define DESERIALIZE_FIELD_STRING(field) \
        if (!reader.readString(field)) return false;

#define DESERIALIZE_END() \
        return true; \
    }

} // namespace utils
} // namespace apollo
