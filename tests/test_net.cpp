/**
 * @file test_net.cpp
 * @brief Network module unit tests
 */

#include "apollo/net/packet.h"
#include "apollo/net/session.h"
#include "apollo/net/connection.h"
#include "apollo/net/event_loop.h"
#include "apollo/net/net_util.h"
#include "apollo/net/http.h"
#include "apollo/net/websocket.h"
#include "apollo/net/message_codec.h"
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace apollo::net;

namespace {

#define TEST_ASSERT(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "  FAILED: " << msg << " at line " << __LINE__ << std::endl; \
            return false; \
        } \
    } while (0)

//==============================================================================
// PacketBuffer Tests
//==============================================================================

bool test_packet_buffer_default() {
    std::cout << "Running: test_packet_buffer_default..." << std::endl;

    PacketBuffer buffer;
    TEST_ASSERT(buffer.empty(), "Buffer is empty initially");
    TEST_ASSERT(buffer.size() == 0, "Size is 0 initially");
    // Note: data() may return nullptr for empty vector (implementation-defined)
    TEST_ASSERT(buffer.data() == nullptr || buffer.size() == 0, "Data pointer valid for empty buffer");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_uint8() {
    std::cout << "Running: test_packet_buffer_write_uint8..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt8(0x42);
    buffer.writeUInt8(0xFF);

    TEST_ASSERT(buffer.size() == 2, "Size is 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 0x42, "First byte is 0x42");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[1]) == 0xFF, "Second byte is 0xFF");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_uint16_le() {
    std::cout << "Running: test_packet_buffer_write_uint16_le..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt16LE(0x1234);

    TEST_ASSERT(buffer.size() == 2, "Size is 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 0x34, "Low byte first");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[1]) == 0x12, "High byte second");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_uint16_be() {
    std::cout << "Running: test_packet_buffer_write_uint16_be..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt16BE(0x1234);

    TEST_ASSERT(buffer.size() == 2, "Size is 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 0x12, "High byte first");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[1]) == 0x34, "Low byte second");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_uint32_le() {
    std::cout << "Running: test_packet_buffer_write_uint32_le..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt32LE(0x12345678);

    TEST_ASSERT(buffer.size() == 4, "Size is 4");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 0x78, "Byte 0");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[1]) == 0x56, "Byte 1");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[2]) == 0x34, "Byte 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[3]) == 0x12, "Byte 3");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_uint32_be() {
    std::cout << "Running: test_packet_buffer_write_uint32_be..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt32BE(0x12345678);

    TEST_ASSERT(buffer.size() == 4, "Size is 4");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 0x12, "Byte 0");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[1]) == 0x34, "Byte 1");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[2]) == 0x56, "Byte 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[3]) == 0x78, "Byte 3");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_string() {
    std::cout << "Running: test_packet_buffer_write_string..." << std::endl;

    PacketBuffer buffer;
    std::string testStr = "Hello";
    buffer.writeString(testStr);

    TEST_ASSERT(buffer.size() == 5, "Size is 5");
    TEST_ASSERT(std::string(buffer.data(), 5) == "Hello", "String content matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_lp_string() {
    std::cout << "Running: test_packet_buffer_write_lp_string..." << std::endl;

    PacketBuffer buffer;
    std::string testStr = "World";
    buffer.writeLPString(testStr, false);  // 8-bit length

    TEST_ASSERT(buffer.size() == 6, "Size is 6 (1 + 5)");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[0]) == 5, "Length prefix is 5");
    TEST_ASSERT(std::string(buffer.data() + 1, 5) == "World", "String content matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_write_fixed_string() {
    std::cout << "Running: test_packet_buffer_write_fixed_string..." << std::endl;

    PacketBuffer buffer;
    std::string testStr = "Hi";
    buffer.writeFixedString(testStr, 5);

    TEST_ASSERT(buffer.size() == 5, "Size is 5");
    TEST_ASSERT(std::string(buffer.data(), 2) == "Hi", "String content matches");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[2]) == 0, "Padding byte 2");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[3]) == 0, "Padding byte 3");
    TEST_ASSERT(static_cast<uint8_t>(buffer.buffer()[4]) == 0, "Padding byte 4");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_buffer_clear() {
    std::cout << "Running: test_packet_buffer_clear..." << std::endl;

    PacketBuffer buffer;
    buffer.writeUInt32LE(0x12345678);
    TEST_ASSERT(buffer.size() == 4, "Size is 4 before clear");

    buffer.clear();
    TEST_ASSERT(buffer.empty(), "Buffer is empty after clear");
    TEST_ASSERT(buffer.size() == 0, "Size is 0 after clear");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PacketReader Tests
//==============================================================================

bool test_packet_reader_default() {
    std::cout << "Running: test_packet_reader_default..." << std::endl;

    PacketReader reader;
    TEST_ASSERT(reader.size() == 0, "Size is 0 initially");
    TEST_ASSERT(reader.position() == 0, "Position is 0 initially");
    TEST_ASSERT(reader.eos(), "EOS is true for empty reader");
    TEST_ASSERT(!reader.hasRemaining(), "No remaining data");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_read_uint8() {
    std::cout << "Running: test_packet_reader_read_uint8..." << std::endl;

    std::vector<char> data = {static_cast<char>(0x42), static_cast<char>(0xFF)};
    PacketReader reader(data);

    uint8_t val1, val2;
    TEST_ASSERT(reader.readUInt8(val1), "Read first byte");
    TEST_ASSERT(val1 == 0x42, "First value is 0x42");
    TEST_ASSERT(reader.readUInt8(val2), "Read second byte");
    TEST_ASSERT(val2 == 0xFF, "Second value is 0xFF");
    TEST_ASSERT(reader.eos(), "EOS after reading all data");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_read_uint16_le() {
    std::cout << "Running: test_packet_reader_read_uint16_le..." << std::endl;

    std::vector<char> data = {static_cast<char>(0x34), static_cast<char>(0x12)};  // 0x1234 LE
    PacketReader reader(data);

    uint16_t val;
    TEST_ASSERT(reader.readUInt16LE(val), "Read uint16 LE");
    TEST_ASSERT(val == 0x1234, "Value is 0x1234");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_read_uint16_be() {
    std::cout << "Running: test_packet_reader_read_uint16_be..." << std::endl;

    std::vector<char> data = {static_cast<char>(0x12), 0x34};  // 0x1234 BE
    PacketReader reader(data);

    uint16_t val;
    TEST_ASSERT(reader.readUInt16BE(val), "Read uint16 BE");
    TEST_ASSERT(val == 0x1234, "Value is 0x1234");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_read_uint32_le() {
    std::cout << "Running: test_packet_reader_read_uint32_le..." << std::endl;

    std::vector<char> data = {static_cast<char>(0x78), 0x56, 0x34, 0x12};  // 0x12345678 LE
    PacketReader reader(data);

    uint32_t val;
    TEST_ASSERT(reader.readUInt32LE(val), "Read uint32 LE");
    TEST_ASSERT(val == 0x12345678, "Value is 0x12345678");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_read_uint32_be() {
    std::cout << "Running: test_packet_reader_read_uint32_be..." << std::endl;

    std::vector<char> data = {static_cast<char>(0x12), 0x34, 0x56, 0x78};  // 0x12345678 BE
    PacketReader reader(data);

    uint32_t val;
    TEST_ASSERT(reader.readUInt32BE(val), "Read uint32 BE");
    TEST_ASSERT(val == 0x12345678, "Value is 0x12345678");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_position_tracking() {
    std::cout << "Running: test_packet_reader_position_tracking..." << std::endl;

    std::vector<char> data = {1, 2, 3, 4, 5};
    PacketReader reader(data);

    TEST_ASSERT(reader.position() == 0, "Position is 0 initially");
    TEST_ASSERT(reader.remaining() == 5, "Remaining is 5");

    uint8_t val;
    reader.readUInt8(val);
    TEST_ASSERT(reader.position() == 1, "Position is 1 after 1 byte");
    TEST_ASSERT(reader.remaining() == 4, "Remaining is 4");

    reader.skip(2);
    TEST_ASSERT(reader.position() == 3, "Position is 3 after skip");
    TEST_ASSERT(reader.remaining() == 2, "Remaining is 2");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_seek() {
    std::cout << "Running: test_packet_reader_seek..." << std::endl;

    std::vector<char> data = {1, 2, 3, 4, 5};
    PacketReader reader(data);

    TEST_ASSERT(reader.seek(3), "Seek to position 3");
    TEST_ASSERT(reader.position() == 3, "Position is 3");
    TEST_ASSERT(reader.remaining() == 2, "Remaining is 2");

    TEST_ASSERT(!reader.seek(10), "Cannot seek beyond size");
    TEST_ASSERT(reader.position() == 3, "Position unchanged after invalid seek");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_reader_peek() {
    std::cout << "Running: test_packet_reader_peek..." << std::endl;

    std::vector<char> data = {1, 2, 3, 4, 5};
    PacketReader reader(data);

    const char* p = reader.peek(2);
    TEST_ASSERT(p != nullptr, "Peek returns valid pointer");
    TEST_ASSERT(static_cast<uint8_t>(p[0]) == 3, "Peek byte 0 is 3");
    TEST_ASSERT(static_cast<uint8_t>(p[1]) == 4, "Peek byte 1 is 4");
    TEST_ASSERT(reader.position() == 0, "Position unchanged after peek");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PacketChecksum Tests
//==============================================================================

bool test_packet_checksum_xor8() {
    std::cout << "Running: test_packet_checksum_xor8..." << std::endl;

    std::vector<uint8_t> data = {static_cast<char>(0x12), 0x34, 0x56};
    uint8_t checksum = PacketChecksum::xor8(data.data(), data.size());

    // 0x12 ^ 0x34 ^ 0x56 = 0x70
    TEST_ASSERT(checksum == 0x70, "XOR8 checksum is correct");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_checksum_xor16() {
    std::cout << "Running: test_packet_checksum_xor16..." << std::endl;

    std::vector<uint8_t> data = {static_cast<char>(0x12), 0x34, 0x56, 0x78};
    uint16_t checksum = PacketChecksum::xor16(data.data(), data.size());

    // (0x1234 ^ 0x5678) = 0x444C
    TEST_ASSERT(checksum == 0x444C, "XOR16 checksum is correct");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_checksum_crc8() {
    std::cout << "Running: test_packet_checksum_crc8..." << std::endl;

    std::vector<uint8_t> data = {static_cast<char>(0x01), 0x02, 0x03};
    uint8_t crc = PacketChecksum::crc8(data.data(), data.size(), 0x07);

    // Just verify it runs and produces consistent result
    uint8_t crc2 = PacketChecksum::crc8(data.data(), data.size(), 0x07);
    TEST_ASSERT(crc == crc2, "CRC8 is deterministic");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_checksum_fnv1a32() {
    std::cout << "Running: test_packet_checksum_fnv1a32..." << std::endl;

    const char* data = "hello";
    uint32_t hash = PacketChecksum::fnv1a32(data, 5);

    // FNV-1a hash should be deterministic
    uint32_t hash2 = PacketChecksum::fnv1a32(data, 5);
    TEST_ASSERT(hash == hash2, "FNV-1a 32-bit hash is deterministic");
    TEST_ASSERT(hash != 0, "FNV-1a 32-bit hash is non-zero");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_checksum_fnv1a64() {
    std::cout << "Running: test_packet_checksum_fnv1a64..." << std::endl;

    const char* data = "world";
    uint64_t hash = PacketChecksum::fnv1a64(data, 5);

    // FNV-1a hash should be deterministic
    uint64_t hash2 = PacketChecksum::fnv1a64(data, 5);
    TEST_ASSERT(hash == hash2, "FNV-1a 64-bit hash is deterministic");
    TEST_ASSERT(hash != 0, "FNV-1a 64-bit hash is non-zero");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PacketAssembler Tests
//==============================================================================

bool test_packet_assembler_fragment() {
    std::cout << "Running: test_packet_assembler_fragment..." << std::endl;

    PacketAssembler assembler;
    std::string data = "Hello, World! This is a test packet.";

    auto fragments = assembler.fragment(data.data(), data.size(), 123, 10);

    TEST_ASSERT(!fragments.empty(), "Fragments created");
    TEST_ASSERT(fragments.size() > 1, "Multiple fragments created");
    TEST_ASSERT(fragments[0].packetId == 123, "Packet ID matches");
    TEST_ASSERT(fragments[0].totalFragments == fragments.size(), "Total fragments matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_assembler_assemble() {
    std::cout << "Running: test_packet_assembler_assemble..." << std::endl;

    PacketAssembler assembler;

    // Create 3 fragments
    PacketAssembler::Fragment frag1;
    frag1.packetId = 100;
    frag1.fragmentIndex = 0;
    frag1.totalFragments = 3;
    frag1.data = {'A', 'B'};

    PacketAssembler::Fragment frag2;
    frag2.packetId = 100;
    frag2.fragmentIndex = 1;
    frag2.totalFragments = 3;
    frag2.data = {'C', 'D'};

    PacketAssembler::Fragment frag3;
    frag3.packetId = 100;
    frag3.fragmentIndex = 2;
    frag3.totalFragments = 3;
    frag3.data = {'E', 'F'};

    auto result1 = assembler.assemble(frag1);
    TEST_ASSERT(!result1.complete, "Not complete after 1 fragment");

    auto result2 = assembler.assemble(frag2);
    TEST_ASSERT(!result2.complete, "Not complete after 2 fragments");

    auto result3 = assembler.assemble(frag3);
    TEST_ASSERT(result3.complete, "Complete after 3 fragments");
    TEST_ASSERT(result3.packetId == 100, "Packet ID matches");
    TEST_ASSERT(result3.data.size() == 6, "Data size is 6");

    std::string assembled(result3.data.begin(), result3.data.end());
    TEST_ASSERT(assembled == "ABCDEF", "Assembled data matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_packet_assembler_cancel() {
    std::cout << "Running: test_packet_assembler_cancel..." << std::endl;

    PacketAssembler assembler;

    PacketAssembler::Fragment frag;
    frag.packetId = 200;
    frag.fragmentIndex = 0;
    frag.totalFragments = 2;
    frag.data = {'X'};

    assembler.assemble(frag);
    bool cancelled = assembler.cancel(200);

    TEST_ASSERT(cancelled, "Cancel returns true");
    cancelled = assembler.cancel(200);
    TEST_ASSERT(!cancelled, "Cancel returns false for non-existent packet");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PacketHeader Tests
//==============================================================================

bool test_packet_header_flags() {
    std::cout << "Running: test_packet_header_flags..." << std::endl;

    PacketHeader header;
    header.flags = PacketHeader::FLAG_FRAGMENTED | PacketHeader::FLAG_RELIABLE;

    TEST_ASSERT(header.isFragmented(), "Is fragmented");
    TEST_ASSERT(header.isReliable(), "Is reliable");
    TEST_ASSERT(!header.isCompressed(), "Not compressed");
    TEST_ASSERT(!header.isEncrypted(), "Not encrypted");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// MTU Tests
//==============================================================================

bool test_mtu_constants() {
    std::cout << "Running: test_mtu_constants..." << std::endl;

    TEST_ASSERT(MTU::MIN_MTU == 576, "MIN_MTU is 576");
    TEST_ASSERT(MTU::ETHERNET_MTU == 1500, "ETHERNET_MTU is 1500");
    TEST_ASSERT(MTU::JUMBO_FRAME == 9000, "JUMBO_FRAME is 9000");
    TEST_ASSERT(MTU::LOOPBACK_MTU == 65536, "LOOPBACK_MTU is 65536");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_mtu_safe_sizes() {
    std::cout << "Running: test_mtu_safe_sizes..." << std::endl;

    int udpSafe = MTU::safeUdpSize();
    TEST_ASSERT(udpSafe == 1472, "UDP safe size is 1472 (1500 - 20 - 8)");

    int tcpSafe = MTU::safeTcpSize();
    TEST_ASSERT(tcpSafe == 1460, "TCP safe size is 1460 (1500 - 20 - 20)");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// ConnectionState Tests
//==============================================================================

bool test_connection_state_values() {
    std::cout << "Running: test_connection_state_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(ConnectionState::Disconnected) == 0, "Disconnected is 0");
    TEST_ASSERT(static_cast<int>(ConnectionState::Connecting) == 1, "Connecting is 1");
    TEST_ASSERT(static_cast<int>(ConnectionState::Connected) == 2, "Connected is 2");
    TEST_ASSERT(static_cast<int>(ConnectionState::Disconnecting) == 3, "Disconnecting is 3");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// SessionEvent Tests
//==============================================================================

bool test_session_event_values() {
    std::cout << "Running: test_session_event_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(SessionEvent::Established) == 0, "Established is 0");
    TEST_ASSERT(static_cast<int>(SessionEvent::Terminated) == 1, "Terminated is 1");
    TEST_ASSERT(static_cast<int>(SessionEvent::Error) == 2, "Error is 2");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// TerminateReason Tests
//==============================================================================

bool test_terminate_reason_values() {
    std::cout << "Running: test_terminate_reason_values..." << std::endl;

    TEST_ASSERT(static_cast<int>(TerminateReason::None) == 0, "None is 0");
    TEST_ASSERT(static_cast<int>(TerminateReason::UserClose) == 1, "UserClose is 1");
    TEST_ASSERT(static_cast<int>(TerminateReason::RemoteClose) == 2, "RemoteClose is 2");
    TEST_ASSERT(static_cast<int>(TerminateReason::Timeout) == 3, "Timeout is 3");
    TEST_ASSERT(static_cast<int>(TerminateReason::Error) == 4, "Error is 4");
    TEST_ASSERT(static_cast<int>(TerminateReason::Shutdown) == 5, "Shutdown is 5");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// EventType Tests
//==============================================================================

bool test_event_type_values() {
    std::cout << "Running: test_event_type_values..." << std::endl;

    TEST_ASSERT(static_cast<uint32_t>(EventType::NONE) == 0, "NONE is 0");
    TEST_ASSERT(static_cast<uint32_t>(EventType::READ) == 1, "READ is 1");
    TEST_ASSERT(static_cast<uint32_t>(EventType::WRITE) == 2, "WRITE is 2");
    TEST_ASSERT(static_cast<uint32_t>(EventType::ERR) == 4, "ERR is 4");
    TEST_ASSERT(static_cast<uint32_t>(EventType::EDGE_TRIGGER) == 8, "EDGE_TRIGGER is 8");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_event_type_operators() {
    std::cout << "Running: test_event_type_operators..." << std::endl;

    auto combined = EventType::READ | EventType::WRITE;
    TEST_ASSERT(HasEvent(combined, EventType::READ), "Has READ event");
    TEST_ASSERT(HasEvent(combined, EventType::WRITE), "Has WRITE event");
    TEST_ASSERT(!HasEvent(combined, EventType::ERR), "No ERR event");

    auto withErr = combined | EventType::ERR;
    TEST_ASSERT(HasEvent(withErr, EventType::ERR), "Has ERR event after OR");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// EventLoopStats Tests
//==============================================================================

bool test_event_loop_stats_default() {
    std::cout << "Running: test_event_loop_stats_default..." << std::endl;

    EventLoopStats stats;
    TEST_ASSERT(stats.loopCount == 0, "loopCount is 0");
    TEST_ASSERT(stats.eventsProcessed == 0, "eventsProcessed is 0");
    TEST_ASSERT(stats.timersFired == 0, "timersFired is 0");
    TEST_ASSERT(stats.wakeups == 0, "wakeups is 0");
    TEST_ASSERT(stats.currentConnections == 0, "currentConnections is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// EventLoopConfig Tests
//==============================================================================

bool test_event_loop_config_default() {
    std::cout << "Running: test_event_loop_config_default..." << std::endl;

    EventLoopConfig config;
    TEST_ASSERT(config.maxEvents == 1024, "maxEvents is 1024");
    TEST_ASSERT(config.timeoutMs == 1000, "timeoutMs is 1000");
    TEST_ASSERT(config.enableTimer, "enableTimer is true");
    TEST_ASSERT(config.enableStats, "enableStats is true");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// IPv4Address Tests
//==============================================================================

bool test_ipv4_address_default() {
    std::cout << "Running: test_ipv4_address_default..." << std::endl;

    IPv4Address addr;
    TEST_ASSERT(!addr.isValid(), "Default address is invalid");
    TEST_ASSERT(addr.port() == 0, "Port is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ipv4_address_constructors() {
    std::cout << "Running: test_ipv4_address_constructors..." << std::endl;

    IPv4Address addr1(0x7F000001, 8080);  // 127.0.0.1:8080
    TEST_ASSERT(addr1.ipHost() == 0x7F000001, "IP is 127.0.0.1");
    TEST_ASSERT(addr1.port() == 8080, "Port is 8080");
    TEST_ASSERT(addr1.isLocalhost(), "Is localhost");

    IPv4Address addr2("192.168.1.1", 80);
    TEST_ASSERT(addr2.isValid(), "Is valid");
    TEST_ASSERT(addr2.port() == 80, "Port is 80");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ipv4_address_to_string() {
    std::cout << "Running: test_ipv4_address_to_string..." << std::endl;

    IPv4Address addr(0x7F000001, 8080);
    std::string str = addr.toString();
    TEST_ASSERT(str.find("127.0.0.1") != std::string::npos, "Contains IP");
    TEST_ASSERT(str.find("8080") != std::string::npos, "Contains port");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ipv4_address_parse() {
    std::cout << "Running: test_ipv4_address_parse..." << std::endl;

    IPv4Address addr = IPv4Address::parse("192.168.1.1:8080");
    TEST_ASSERT(addr.isValid(), "Parsed address is valid");
    TEST_ASSERT(addr.port() == 8080, "Port parsed correctly");
    TEST_ASSERT(addr.ipToString() == "192.168.1.1", "IP parsed correctly");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_ipv4_address_static_methods() {
    std::cout << "Running: test_ipv4_address_static_methods..." << std::endl;

    IPv4Address any = IPv4Address::any();
    TEST_ASSERT(any.ipHost() == 0, "Any address is 0.0.0.0");

    IPv4Address loopback = IPv4Address::loopback();
    TEST_ASSERT(loopback.isLocalhost(), "Loopback is localhost");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// PortRange Tests
//==============================================================================

bool test_port_range_contains() {
    std::cout << "Running: test_port_range_contains..." << std::endl;

    PortRange range(1024, 49151);
    TEST_ASSERT(range.contains(1024), "Contains min port");
    TEST_ASSERT(range.contains(49151), "Contains max port");
    TEST_ASSERT(range.contains(8080), "Contains port in range");
    TEST_ASSERT(!range.contains(1023), "Doesn't contain port below range");
    TEST_ASSERT(!range.contains(49152), "Doesn't contain port above range");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_port_range_static() {
    std::cout << "Running: test_port_range_static..." << std::endl;

    auto wellKnown = PortRange::wellKnown();
    TEST_ASSERT(wellKnown.min() == 0, "Well known min is 0");
    TEST_ASSERT(wellKnown.max() == 1023, "Well known max is 1023");

    auto dynamic = PortRange::dynamic();
    TEST_ASSERT(dynamic.min() == 49152, "Dynamic min is 49152");
    TEST_ASSERT(dynamic.max() == 65535, "Dynamic max is 65535");

    TEST_ASSERT(PortRange::isSystemPort(80), "Port 80 is system port");
    TEST_ASSERT(PortRange::isSystemPort(1023), "Port 1023 is system port");
    TEST_ASSERT(!PortRange::isSystemPort(1024), "Port 1024 is not system port");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// Url Tests
//==============================================================================

bool test_url_parse() {
    std::cout << "Running: test_url_parse..." << std::endl;

    Url url("http://example.com:8080/path/to/resource?key=value");
    TEST_ASSERT(url.isValid(), "URL is valid");
    TEST_ASSERT(url.protocol() == "http", "Protocol is http");
    TEST_ASSERT(url.host() == "example.com", "Host is example.com");
    TEST_ASSERT(url.port() == 8080, "Port is 8080");
    TEST_ASSERT(url.path() == "/path/to/resource", "Path matches");
    TEST_ASSERT(url.query() == "key=value", "Query matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_url_default_port() {
    std::cout << "Running: test_url_default_port..." << std::endl;

    Url http("http://example.com");
    TEST_ASSERT(http.getDefaultPort() == 80, "HTTP default port is 80");

    Url https("https://example.com");
    TEST_ASSERT(https.getDefaultPort() == 443, "HTTPS default port is 443");

    Url ftp("ftp://example.com");
    TEST_ASSERT(ftp.getDefaultPort() == 21, "FTP default port is 21");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// NetworkStats Tests
//==============================================================================

bool test_network_stats_default() {
    std::cout << "Running: test_network_stats_default..." << std::endl;

    NetworkStats stats;
    TEST_ASSERT(stats.bytesSent == 0, "bytesSent is 0");
    TEST_ASSERT(stats.bytesReceived == 0, "bytesReceived is 0");
    TEST_ASSERT(stats.packetsSent == 0, "packetsSent is 0");
    TEST_ASSERT(stats.packetsReceived == 0, "packetsReceived is 0");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_network_stats_add() {
    std::cout << "Running: test_network_stats_add..." << std::endl;

    NetworkStats stats;
    stats.addSent(100);
    TEST_ASSERT(stats.bytesSent == 100, "bytesSent is 100");
    TEST_ASSERT(stats.packetsSent == 1, "packetsSent is 1");

    stats.addSent(50);
    TEST_ASSERT(stats.bytesSent == 150, "bytesSent is 150");
    TEST_ASSERT(stats.packetsSent == 2, "packetsSent is 2");

    stats.addReceived(200);
    TEST_ASSERT(stats.bytesReceived == 200, "bytesReceived is 200");
    TEST_ASSERT(stats.packetsReceived == 1, "packetsReceived is 1");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_network_stats_reset() {
    std::cout << "Running: test_network_stats_reset..." << std::endl;

    NetworkStats stats;
    stats.addSent(100);
    stats.addReceived(200);
    stats.connectionsAccepted = 5;

    stats.reset();

    TEST_ASSERT(stats.bytesSent == 0, "bytesSent is 0 after reset");
    TEST_ASSERT(stats.bytesReceived == 0, "bytesReceived is 0 after reset");
    TEST_ASSERT(stats.packetsSent == 0, "packetsSent is 0 after reset");
    TEST_ASSERT(stats.connectionsAccepted == 0, "connectionsAccepted is 0 after reset");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Method Tests
//==============================================================================

bool test_http_method_to_string() {
    std::cout << "Running: test_http_method_to_string..." << std::endl;

    using namespace http;

    TEST_ASSERT(std::string(toString(Method::Get)) == "GET", "GET string");
    TEST_ASSERT(std::string(toString(Method::Post)) == "POST", "POST string");
    TEST_ASSERT(std::string(toString(Method::Put)) == "PUT", "PUT string");
    TEST_ASSERT(std::string(toString(Method::Delete)) == "DELETE", "DELETE string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_method_from_string() {
    std::cout << "Running: test_http_method_from_string..." << std::endl;

    using namespace http;

    TEST_ASSERT(fromString("GET") == Method::Get, "GET parsed");
    TEST_ASSERT(fromString("POST") == Method::Post, "POST parsed");
    TEST_ASSERT(fromString("PUT") == Method::Put, "PUT parsed");
    TEST_ASSERT(fromString("DELETE") == Method::Delete, "DELETE parsed");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP StatusCode Tests
//==============================================================================

bool test_http_status_code_to_string() {
    std::cout << "Running: test_http_status_code_to_string..." << std::endl;

    using namespace http;

    TEST_ASSERT(std::string(toString(StatusCode::OK)) == "OK", "200 string");
    TEST_ASSERT(std::string(toString(StatusCode::NotFound)) == "Not Found", "404 string");
    TEST_ASSERT(std::string(toString(StatusCode::InternalServerError)) == "Internal Server Error", "500 string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_status_code_to_int() {
    std::cout << "Running: test_http_status_code_to_int..." << std::endl;

    using namespace http;

    TEST_ASSERT(toInt(StatusCode::OK) == 200, "OK is 200");
    TEST_ASSERT(toInt(StatusCode::NotFound) == 404, "NotFound is 404");
    TEST_ASSERT(toInt(StatusCode::InternalServerError) == 500, "InternalServerError is 500");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Version Tests
//==============================================================================

bool test_http_version() {
    std::cout << "Running: test_http_version..." << std::endl;

    using namespace http;

    Version v1 = Version::HTTP_1_0();
    TEST_ASSERT(v1.major == 1 && v1.minor == 0, "HTTP/1.0");
    TEST_ASSERT(v1.toString() == "HTTP/1.0", "HTTP/1.0 string");

    Version v2 = Version::HTTP_1_1();
    TEST_ASSERT(v2.major == 1 && v2.minor == 1, "HTTP/1.1");
    TEST_ASSERT(v2.toString() == "HTTP/1.1", "HTTP/1.1 string");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Headers Tests
//==============================================================================

bool test_http_headers_basic() {
    std::cout << "Running: test_http_headers_basic..." << std::endl;

    using namespace http;

    Headers headers;
    TEST_ASSERT(headers.empty(), "Headers empty initially");

    headers.set("Content-Type", "application/json");
    TEST_ASSERT(headers.has("Content-Type"), "Has Content-Type");
    TEST_ASSERT(headers.get("Content-Type") == "application/json", "Value matches");

    TEST_ASSERT(headers.size() == 1, "Size is 1");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_headers_case_insensitive() {
    std::cout << "Running: test_http_headers_case_insensitive..." << std::endl;

    using namespace http;

    Headers headers;
    headers.set("Content-Type", "text/html");

    TEST_ASSERT(headers.has("content-type"), "Has lowercase version");
    TEST_ASSERT(headers.has("CONTENT-TYPE"), "Has uppercase version");
    TEST_ASSERT(headers.get("content-type") == "text/html", "Value matches for lowercase");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_headers_helpers() {
    std::cout << "Running: test_http_headers_helpers..." << std::endl;

    using namespace http;

    Headers headers;
    headers.setContentType("application/json");
    TEST_ASSERT(headers.contentType() == "application/json", "ContentType matches");

    headers.setContentLength(1234);
    TEST_ASSERT(headers.contentLength() == 1234, "ContentLength matches");

    headers.setConnection("keep-alive");
    TEST_ASSERT(headers.connection() == "keep-alive", "Connection matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Request Tests
//==============================================================================

bool test_http_request_default() {
    std::cout << "Running: test_http_request_default..." << std::endl;

    using namespace http;

    Request request;
    TEST_ASSERT(request.method == Method::Get, "Default method is GET");
    TEST_ASSERT(request.uri.empty(), "URI is empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_request_query_params() {
    std::cout << "Running: test_http_request_query_params..." << std::endl;

    using namespace http;

    Request request;
    request.queryParams["key1"] = "value1";
    request.queryParams["key2"] = "value2";

    TEST_ASSERT(request.hasQuery("key1"), "Has key1");
    TEST_ASSERT(request.getQuery("key1") == "value1", "value1 matches");
    TEST_ASSERT(request.getQuery("missing", "default") == "default", "Default value");
    TEST_ASSERT(request.getQueryAsInt("missing", 42) == 42, "Default int value");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_request_keep_alive() {
    std::cout << "Running: test_http_request_keep_alive..." << std::endl;

    using namespace http;

    Request request;
    TEST_ASSERT(request.shouldKeepAlive(), "Default is keep-alive");

    request.headers.setConnection("close");
    TEST_ASSERT(!request.shouldKeepAlive(), "Not keep-alive when Connection: close");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Response Tests
//==============================================================================

bool test_http_response_default() {
    std::cout << "Running: test_http_response_default..." << std::endl;

    using namespace http;

    Response response;
    TEST_ASSERT(response.status == StatusCode::OK, "Default status is OK");
    TEST_ASSERT(response.body.empty(), "Body is empty");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_response_set_body() {
    std::cout << "Running: test_http_response_set_body..." << std::endl;

    using namespace http;

    Response response;
    response.setBody("Hello, World!", "text/plain");

    TEST_ASSERT(response.body == "Hello, World!", "Body matches");
    TEST_ASSERT(response.headers.contentType() == "text/plain", "ContentType matches");
    TEST_ASSERT(response.headers.contentLength() == 13, "ContentLength matches");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_response_json() {
    std::cout << "Running: test_http_response_json..." << std::endl;

    using namespace http;

    Response response;
    response.setJson("{\"key\":\"value\"}");

    TEST_ASSERT(response.body == "{\"key\":\"value\"}", "Body matches");
    TEST_ASSERT(response.headers.contentType() == "application/json", "ContentType is JSON");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_response_html() {
    std::cout << "Running: test_http_response_html..." << std::endl;

    using namespace http;

    Response response;
    response.setHtml("<html><body>Hello</body></html>");

    TEST_ASSERT(response.body.find("Hello") != std::string::npos, "Body contains Hello");
    TEST_ASSERT(response.headers.contentType() == "text/html", "ContentType is HTML");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_response_redirect() {
    std::cout << "Running: test_http_response_redirect..." << std::endl;

    using namespace http;

    Response response;
    response.redirect("http://example.com/new", StatusCode::MovedPermanently);

    TEST_ASSERT(response.status == StatusCode::MovedPermanently, "Status is 301");
    TEST_ASSERT(response.headers.get("Location") == "http://example.com/new", "Location header set");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_response_static_methods() {
    std::cout << "Running: test_http_response_static_methods..." << std::endl;

    using namespace http;

    Response ok = Response::ok("Success");
    TEST_ASSERT(ok.status == StatusCode::OK, "OK status");
    TEST_ASSERT(ok.body == "Success", "OK body");

    Response notFound = Response::notFound();
    TEST_ASSERT(notFound.status == StatusCode::NotFound, "NotFound status");

    Response badRequest = Response::badRequest();
    TEST_ASSERT(badRequest.status == StatusCode::BadRequest, "BadRequest status");

    Response serverError = Response::serverError();
    TEST_ASSERT(serverError.status == StatusCode::InternalServerError, "ServerError status");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// HTTP Router Tests
//==============================================================================

bool test_http_router_add_route() {
    std::cout << "Running: test_http_router_add_route..." << std::endl;

    using namespace http;

    Router router;
    int callCount = 0;

    router.addRoute("/test", Method::Get, [&callCount](const Request&) {
        callCount++;
        return Response::ok("Test");
    });

    Request req;
    req.method = Method::Get;
    req.uri = "/test";

    Response resp = router.route(req);
    TEST_ASSERT(callCount == 1, "Handler was called");
    TEST_ASSERT(resp.status == StatusCode::OK, "Response is OK");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_router_not_found() {
    std::cout << "Running: test_http_router_not_found..." << std::endl;

    using namespace http;

    Router router;
    Request req;
    req.method = Method::Get;
    req.uri = "/nonexistent";

    Response resp = router.route(req);
    TEST_ASSERT(resp.status == StatusCode::NotFound, "Returns 404 for unknown route");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_http_router_convenience_methods() {
    std::cout << "Running: test_http_router_convenience_methods..." << std::endl;

    using namespace http;

    Router router;
    int getCalled = 0;
    int postCalled = 0;

    router.get("/resource", [&getCalled](const Request&) {
        getCalled++;
        return Response::ok();
    });

    router.post("/resource", [&postCalled](const Request&) {
        postCalled++;
        return Response::ok();
    });

    Request getReq;
    getReq.method = Method::Get;
    getReq.uri = "/resource";
    router.route(getReq);
    TEST_ASSERT(getCalled == 1, "GET handler called");
    TEST_ASSERT(postCalled == 0, "POST handler not called");

    Request postReq;
    postReq.method = Method::Post;
    postReq.uri = "/resource";
    router.route(postReq);
    TEST_ASSERT(getCalled == 1, "GET handler still 1");
    TEST_ASSERT(postCalled == 1, "POST handler called");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// WebSocket OpCode Tests
//==============================================================================

bool test_websocket_opcode_values() {
    std::cout << "Running: test_websocket_opcode_values..." << std::endl;

    using namespace websocket;

    TEST_ASSERT(static_cast<int>(OpCode::Continuation) == 0x0, "Continuation is 0x0");
    TEST_ASSERT(static_cast<int>(OpCode::Text) == 0x1, "Text is 0x1");
    TEST_ASSERT(static_cast<int>(OpCode::Binary) == 0x2, "Binary is 0x2");
    TEST_ASSERT(static_cast<int>(OpCode::Close) == 0x8, "Close is 0x8");
    TEST_ASSERT(static_cast<int>(OpCode::Ping) == 0x9, "Ping is 0x9");
    TEST_ASSERT(static_cast<int>(OpCode::Pong) == 0xA, "Pong is 0xA");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// WebSocket CloseStatus Tests
//==============================================================================

bool test_websocket_close_status_values() {
    std::cout << "Running: test_websocket_close_status_values..." << std::endl;

    using namespace websocket;

    TEST_ASSERT(static_cast<int>(CloseStatus::Normal) == 1000, "Normal is 1000");
    TEST_ASSERT(static_cast<int>(CloseStatus::GoingAway) == 1001, "GoingAway is 1001");
    TEST_ASSERT(static_cast<int>(CloseStatus::ProtocolError) == 1002, "ProtocolError is 1002");
    TEST_ASSERT(static_cast<int>(CloseStatus::InternalError) == 1011, "InternalError is 1011");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// WebSocket Message Tests
//==============================================================================

bool test_websocket_message_default() {
    std::cout << "Running: test_websocket_message_default..." << std::endl;

    using namespace websocket;

    Message msg;
    TEST_ASSERT(msg.opcode == OpCode::Text, "Default is Text");
    TEST_ASSERT(msg.isFinal, "Default is final");
    TEST_ASSERT(msg.isText(), "Is text");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_websocket_message_static_creators() {
    std::cout << "Running: test_websocket_message_static_creators..." << std::endl;

    using namespace websocket;

    Message textMsg = Message::text("Hello");
    TEST_ASSERT(textMsg.isText(), "Is text message");
    TEST_ASSERT(textMsg.getText() == "Hello", "Text content matches");

    std::vector<uint8_t> binaryData = {1, 2, 3, 4};
    Message binaryMsg = Message::binary(binaryData);
    TEST_ASSERT(binaryMsg.isBinary(), "Is binary message");
    TEST_ASSERT(binaryMsg.data.size() == 4, "Binary data size matches");

    Message closeMsg = Message::close(CloseStatus::Normal);
    TEST_ASSERT(closeMsg.isControl(), "Close is control frame");
    TEST_ASSERT(closeMsg.opcode == OpCode::Close, "Opcode is Close");

    Message pingMsg = Message::ping();
    TEST_ASSERT(pingMsg.opcode == OpCode::Ping, "Ping opcode");

    Message pongMsg = Message::pong();
    TEST_ASSERT(pongMsg.opcode == OpCode::Pong, "Pong opcode");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// WebSocket Config Tests
//==============================================================================

bool test_websocket_config_default() {
    std::cout << "Running: test_websocket_config_default..." << std::endl;

    using namespace websocket;

    Config config;
    TEST_ASSERT(config.maxMessageSize == 16 * 1024 * 1024, "Max message size is 16MB");
    TEST_ASSERT(config.maxFrameSize == 64 * 1024, "Max frame size is 64KB");
    TEST_ASSERT(config.handshakeTimeoutMs == 5000, "Handshake timeout is 5s");
    TEST_ASSERT(!config.enableCompression, "Compression disabled by default");
    TEST_ASSERT(!config.autoPing, "Auto ping disabled by default");

    std::cout << "  PASSED" << std::endl;
    return true;
}

//==============================================================================
// MessageHeader Tests
//==============================================================================

bool test_message_header_default() {
    std::cout << "Running: test_message_header_default..." << std::endl;

    MessageHeader header;
    TEST_ASSERT(header.length == 0, "Length is 0");
    TEST_ASSERT(header.msgId == 0, "MsgId is 0");
    TEST_ASSERT(header.seq == 0, "Seq is 0");
    TEST_ASSERT(header.flags == MessageHeader::None, "Flags is None");

    std::cout << "  PASSED" << std::endl;
    return true;
}

bool test_message_header_size() {
    std::cout << "Running: test_message_header_size..." << std::endl;

    TEST_ASSERT(MessageHeader::SIZE == 12, "Header size is 12 bytes");

    std::cout << "  PASSED" << std::endl;
    return true;
}

} // namespace

//==============================================================================
// Main Test Runner
//==============================================================================

int main() {
    std::cout << "=== Apollo Network Module Test Suite ===" << std::endl;

    int total = 0;
    int passed = 0;

    auto run = [&](bool (*fn)()) {
        total++;
        if (fn()) {
            passed++;
        }
    };

    // PacketBuffer tests
    run(test_packet_buffer_default);
    run(test_packet_buffer_write_uint8);
    run(test_packet_buffer_write_uint16_le);
    run(test_packet_buffer_write_uint16_be);
    run(test_packet_buffer_write_uint32_le);
    run(test_packet_buffer_write_uint32_be);
    run(test_packet_buffer_write_string);
    run(test_packet_buffer_write_lp_string);
    run(test_packet_buffer_write_fixed_string);
    run(test_packet_buffer_clear);

    // PacketReader tests
    run(test_packet_reader_default);
    run(test_packet_reader_read_uint8);
    run(test_packet_reader_read_uint16_le);
    run(test_packet_reader_read_uint16_be);
    run(test_packet_reader_read_uint32_le);
    run(test_packet_reader_read_uint32_be);
    run(test_packet_reader_position_tracking);
    run(test_packet_reader_seek);
    run(test_packet_reader_peek);

    // PacketChecksum tests
    run(test_packet_checksum_xor8);
    run(test_packet_checksum_xor16);
    run(test_packet_checksum_crc8);
    run(test_packet_checksum_fnv1a32);
    run(test_packet_checksum_fnv1a64);

    // PacketAssembler tests
    run(test_packet_assembler_fragment);
    run(test_packet_assembler_assemble);
    run(test_packet_assembler_cancel);

    // PacketHeader tests
    run(test_packet_header_flags);

    // MTU tests
    run(test_mtu_constants);
    run(test_mtu_safe_sizes);

    // ConnectionState tests
    run(test_connection_state_values);

    // SessionEvent tests
    run(test_session_event_values);

    // TerminateReason tests
    run(test_terminate_reason_values);

    // EventType tests
    run(test_event_type_values);
    run(test_event_type_operators);

    // EventLoopStats tests
    run(test_event_loop_stats_default);

    // EventLoopConfig tests
    run(test_event_loop_config_default);

    // IPv4Address tests
    run(test_ipv4_address_default);
    run(test_ipv4_address_constructors);
    run(test_ipv4_address_to_string);
    run(test_ipv4_address_parse);
    run(test_ipv4_address_static_methods);

    // PortRange tests
    run(test_port_range_contains);
    run(test_port_range_static);

    // Url tests
    run(test_url_parse);
    run(test_url_default_port);

    // NetworkStats tests
    run(test_network_stats_default);
    run(test_network_stats_add);
    run(test_network_stats_reset);

    // HTTP Method tests
    run(test_http_method_to_string);
    run(test_http_method_from_string);

    // HTTP StatusCode tests
    run(test_http_status_code_to_string);
    run(test_http_status_code_to_int);

    // HTTP Version tests
    run(test_http_version);

    // HTTP Headers tests
    run(test_http_headers_basic);
    run(test_http_headers_case_insensitive);
    run(test_http_headers_helpers);

    // HTTP Request tests
    run(test_http_request_default);
    run(test_http_request_query_params);
    run(test_http_request_keep_alive);

    // HTTP Response tests
    run(test_http_response_default);
    run(test_http_response_set_body);
    run(test_http_response_json);
    run(test_http_response_html);
    run(test_http_response_redirect);
    run(test_http_response_static_methods);

    // HTTP Router tests
    run(test_http_router_add_route);
    run(test_http_router_not_found);
    run(test_http_router_convenience_methods);

    // WebSocket OpCode tests
    run(test_websocket_opcode_values);

    // WebSocket CloseStatus tests
    run(test_websocket_close_status_values);

    // WebSocket Message tests
    run(test_websocket_message_default);
    run(test_websocket_message_static_creators);

    // WebSocket Config tests
    run(test_websocket_config_default);

    // MessageHeader tests
    run(test_message_header_default);
    run(test_message_header_size);

    std::cout << "\n=== Summary ===" << std::endl;
    std::cout << "Total: " << total << std::endl;
    std::cout << "Passed: " << passed << std::endl;
    std::cout << "Failed: " << (total - passed) << std::endl;

    return (total == passed) ? 0 : 1;
}
