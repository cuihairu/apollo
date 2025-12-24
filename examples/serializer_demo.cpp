/**
 * @file serializer_demo.cpp
 * @brief 数据序列化工具使用示例
 */

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include "apollo/utils/serializer.h"

using namespace apollo::utils;

//==============================================================================
// 示例1：基础类型序列化
//==============================================================================

struct PlayerInfo {
    uint64_t playerId;
    std::string name;
    uint8_t level;
    uint32_t exp;
    float hp;
    double mp;
    bool vip;

    SERIALIZE_BEGIN()
        SERIALIZE_FIELD(playerId)
        SERIALIZE_FIELD_STRING(name)
        SERIALIZE_FIELD(level)
        SERIALIZE_FIELD(exp)
        writer.writeFloat(hp)
        writer.writeDouble(mp)
        writer.writeBool(vip)
    SERIALIZE_END()

    DESERIALIZE_BEGIN()
        DESERIALIZE_FIELD(playerId)
        DESERIALIZE_FIELD_STRING(name)
        DESERIALIZE_FIELD(level)
        DESERIALIZE_FIELD(exp)
        if (!reader.readFloat(hp)) return false;
        if (!reader.readDouble(mp)) return false;
        if (!reader.readBool(vip)) return false;
        return true;
    DESERIALIZE_END()
};

void example1_BasicTypes() {
    std::cout << "\n=== Example 1: Basic Types Serialization ===" << std::endl;

    // 写入
    BinaryWriter writer;
    writer.write(static_cast<int32_t>(-12345));
    writer.write(static_cast<uint32_t>(0xDEADBEEF));
    writer.write(static_cast<int64_t>(-9876543210LL));
    writer.writeFloat(3.14159f);
    writer.writeDouble(2.718281828);
    writer.writeBool(true);
    writer.writeString("Hello, Apollo!");

    std::cout << "Written " << writer.size() << " bytes" << std::endl;

    // 读取
    BinaryReader(writer.buffer(), writer.size());

    BinaryReader reader(writer.buffer());

    int32_t i32; uint32_t u32; int64_t i64;
    float f; double d; bool b; std::string str;

    reader.read(i32);
    reader.read(u32);
    reader.read(i64);
    reader.readFloat(f);
    reader.readDouble(d);
    reader.readBool(b);
    reader.readString(str);

    std::cout << "int32: " << i32 << std::endl;
    std::cout << "uint32: 0x" << std::hex << u32 << std::dec << std::endl;
    std::cout << "int64: " << i64 << std::endl;
    std::cout << "float: " << f << std::endl;
    std::cout << "double: " << d << std::endl;
    std::cout << "bool: " << (b ? "true" : "false") << std::endl;
    std::cout << "string: " << str << std::endl;
}

//==============================================================================
// 示例2：结构体序列化
//==============================================================================

void example2_StructSerialization() {
    std::cout << "\n=== Example 2: Struct Serialization ===" << std::endl;

    PlayerInfo original;
    original.playerId = 10001;
    original.name = "DragonSlayer";
    original.level = 55;
    original.exp = 1250000;
    original.hp = 3500.5f;
    original.mp = 1800.75;
    original.vip = true;

    // 序列化
    BinaryWriter writer;
    original.serialize(writer);

    std::cout << "Serialized PlayerInfo: " << writer.size() << " bytes" << std::endl;

    // 反序列化
    PlayerInfo decoded;
    BinaryReader reader(writer.data(), writer.size());
    decoded.deserialize(reader);

    std::cout << "Decoded PlayerInfo:" << std::endl;
    std::cout << "  ID: " << decoded.playerId << std::endl;
    std::cout << "  Name: " << decoded.name << std::endl;
    std::cout << "  Level: " << static_cast<int>(decoded.level) << std::endl;
    std::cout << "  EXP: " << decoded.exp << std::endl;
    std::cout << "  HP: " << decoded.hp << std::endl;
    std::cout << "  MP: " << decoded.mp << std::endl;
    std::cout << "  VIP: " << (decoded.vip ? "Yes" : "No") << std::endl;
}

//==============================================================================
// 示例3：容器序列化
//==============================================================================

void example3_ContainerSerialization() {
    std::cout << "\n=== Example 3: Container Serialization ===" << std::endl;

    // 写入数组
    BinaryWriter writer;

    std::vector<int32_t> scores = {100, 200, 150, 300, 250};
    writer.writeVector(scores);

    std::vector<float> positions = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f};
    writer.writeVector(positions);

    std::vector<std::string> items = {"Sword", "Shield", "Potion"};
    for (const auto& item : items) {
        writer.writeString(item);
    }
    writer.write(static_cast<uint32_t>(items.size()));

    std::cout << "Serialized vectors: " << writer.size() << " bytes" << std::endl;

    // 读取
    BinaryReader reader(writer.data(), writer.size());

    std::vector<int32_t> readScores;
    reader.readVector(readScores);

    std::vector<float> readPositions;
    reader.readVector(readPositions);

    std::cout << "Scores: ";
    for (auto s : readScores) std::cout << s << " ";
    std::cout << std::endl;

    std::cout << "Positions: ";
    for (auto p : readPositions) std::cout << p << " ";
    std::cout << std::endl;
}

//==============================================================================
// 示例4：Varint 编码
//==============================================================================

void example4_VarintEncoding() {
    std::cout << "\n=== Example 4: Varint Encoding ===" << std::endl;

    std::vector<uint32_t> values = {
        0, 1, 127, 128, 300, 16384, 1000000, 0xFFFFFFFF
    };

    for (auto value : values) {
        char buffer[5];
        size_t size = Varint::encodeUInt32(value, buffer);

        std::cout << "Value: " << value << " (" << size << " bytes): ";
        for (size_t i = 0; i < size; ++i) {
            printf("%02X ", static_cast<uint8_t>(buffer[i]));
        }
        std::cout << std::endl;

        // 验证解码
        uint32_t decoded;
        size_t consumed;
        bool ok = Varint::decodeUInt32(buffer, size, decoded, consumed);
        std::cout << "  Decoded: " << decoded << " (ok: " << (ok ? "yes" : "no") << ")" << std::endl;
    }
}

//==============================================================================
// 示例5：ZigZag 编码
//==============================================================================

void example5_ZigZagEncoding() {
    std::cout << "\n=== Example 5: ZigZag Encoding ===" << std::endl;

    std::vector<int32_t> values = {
        0, -1, 1, -2, 2, -127, 127, -128, 128, -12345, 12345
    };

    for (auto value : values) {
        uint32_t encoded = ZigZag::encode32(value);
        int32_t decoded = ZigZag::decode32(encoded);

        std::cout << "Original: " << value
                  << ", Encoded: " << encoded
                  << ", Decoded: " << decoded
                  << (decoded == value ? " ✓" : " ✗") << std::endl;
    }
}

//==============================================================================
// 示例6：协议编解码
//==============================================================================

void example6_ProtocolCodec() {
    std::cout << "\n=== Example 6: Protocol Codec ===" << std::endl;

    // 模拟消息结构: [MsgId(2) | Seq(4) | BodyLen(4) | Body | Checksum(1)]

    enum MessageId : uint16_t {
        MSG_LOGIN = 1001,
        MSG_MOVE = 1002,
        MSG_CHAT = 1003
    };

    // 构建登录消息
    BinaryWriter writer;
    writer.write(static_cast<uint16_t>(MSG_LOGIN));
    writer.write(static_cast<uint32_t>(1)); // seq

    size_t bodyLenPos = writer.size();
    writer.write(static_cast<uint32_t>(0)); // 占位

    // 消息体
    writer.writeString("PlayerName");
    writer.writeString("password123");
    writer.write(static_cast<uint32_t>(10001)); // playerId

    // 填充消息体长度
    uint32_t bodyLen = static_cast<uint32_t>(writer.size() - bodyLenPos - 4);
    uint32_t netLen = hostToNetwork(bodyLen);
    std::memcpy(const_cast<char*>(writer.data()) + bodyLenPos, &netLen, 4);

    // 计算校验和
    uint8_t checksum = ProtocolCodec::calculateChecksum(
        writer.data() + 10, writer.size() - 10); // 跳过头部
    writer.write(checksum);

    std::cout << "Encoded message: " << writer.size() << " bytes" << std::endl;

    // 解码
    BinaryReader reader(writer.data(), writer.size());

    uint16_t msgId; uint32_t seq; uint32_t len;
    reader.read(msgId);
    reader.read(seq);
    reader.read(len);

    std::cout << "MsgId: " << msgId << std::endl;
    std::cout << "Seq: " << seq << std::endl;
    std::cout << "BodyLen: " << len << std::endl;

    std::string name, pass; uint32_t pid;
    reader.readString(name);
    reader.readString(pass);
    reader.read(pid);

    std::cout << "Name: " << name << std::endl;
    std::cout << "Password: " << pass << std::endl;
    std::cout << "PlayerId: " << pid << std::endl;

    uint8_t recvChecksum;
    reader.read(recvChecksum);

    uint8_t calcChecksum = ProtocolCodec::calculateChecksum(
        writer.data() + 10, writer.size() - 10);

    std::cout << "Checksum: " << static_cast<int>(recvChecksum)
              << " (valid: " << (recvChecksum == calcChecksum ? "yes" : "no") << ")" << std::endl;
}

//==============================================================================
// 示例7：位级操作
//==============================================================================

void example7_BitOperations() {
    std::cout << "\n=== Example 7: Bit Operations ===" << std::endl;

    BitWriter writer;

    // 写入位域数据
    writer.writeBits(0b10110, 5);  // 5 bits
    writer.writeBits(0b110011, 6); // 6 bits
    writer.writeBits(0xFF, 8);     // 8 bits
    writer.writeBit(true);
    writer.writeBit(false);
    writer.writeBit(true);

    writer.align();

    std::cout << "BitWriter size: " << writer.size() << " bytes" << std::endl;

    // 读取
    BitReader reader(writer.buffer().data(), writer.size());

    uint32_t v1, v2, v3;
    bool b1, b2, b3;

    reader.readBits(v1, 5);
    reader.readBits(v2, 6);
    reader.readBits(v3, 8);
    reader.readBit(b1);
    reader.readBit(b2);
    reader.readBit(b3);

    std::cout << "v1: " << v1 << " (0b" << std::bitset<5>(v1) << ")" << std::endl;
    std::cout << "v2: " << v2 << " (0b" << std::bitset<6>(v2) << ")" << std::endl;
    std::cout << "v3: " << v3 << " (0b" << std::bitset<8>(v3) << ")" << std::endl;
    std::cout << "b1: " << b1 << ", b2: " << b2 << ", b3: " << b3 << std::endl;
}

//==============================================================================
// 示例8：性能测试
//==============================================================================

void example8_PerformanceTest() {
    std::cout << "\n=== Example 8: Performance Test ===" << std::endl;

    const int iterations = 100000;

    // 序列化测试
    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::vector<char>> buffers;
    buffers.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        BinaryWriter writer;
        writer.write(static_cast<uint32_t>(i));
        writer.write(static_cast<uint64_t>(i * 1000));
        writer.writeFloat(i * 1.5f);
        writer.writeString("Test message");
        buffers.push_back(writer.buffer());
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto serializeTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    // 反序列化测试
    start = std::chrono::high_resolution_clock::now();

    for (auto& buf : buffers) {
        BinaryReader reader(buf.data(), buf.size());
        uint32_t i; uint64_t l; float f; std::string s;
        reader.read(i);
        reader.read(l);
        reader.readFloat(f);
        reader.readString(s);
    }

    end = std::chrono::high_resolution_clock::now();
    auto deserializeTime = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count();

    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Serialize time: " << serializeTime << " us (" << (iterations * 1000000.0 / serializeTime) << " ops/sec)" << std::endl;
    std::cout << "Deserialize time: " << deserializeTime << " us (" << (iterations * 1000000.0 / deserializeTime) << " ops/sec)" << std::endl;
    std::cout << "Avg serialize: " << (serializeTime / iterations) << " ns/op" << std::endl;
    std::cout << "Avg deserialize: " << (deserializeTime / iterations) << " ns/op" << std::endl;
}

//==============================================================================
// 主程序
//==============================================================================

int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "=== Apollo Serializer Demo ===" << std::endl;
    std::cout << "========================================" << std::endl;

    example1_BasicTypes();
    example2_StructSerialization();
    example3_ContainerSerialization();
    example4_VarintEncoding();
    example5_ZigZagEncoding();
    example6_ProtocolCodec();
    example7_BitOperations();
    example8_PerformanceTest();

    std::cout << "\n========================================" << std::endl;
    std::cout << "=== All Examples Complete ===" << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
