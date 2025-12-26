/**
 * @file websocket_demo.cpp
 * @brief WebSocket 服务器和客户端使用示例
 */

#include "apollo/net/websocket.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace apollo;
using namespace apollo::net;
using namespace apollo::net::websocket;

//==============================================================================
// 示例 1: WebSocket 消息创建
//==============================================================================

void messageCreationDemo() {
    std::cout << "\n========== 示例 1: WebSocket 消息创建 ==========\n" << std::endl;

    // 文本消息
    auto textMsg = Message::text("Hello, WebSocket!");
    std::cout << "Text message: " << textMsg.getText() << std::endl;

    // 二进制消息
    std::vector<uint8_t> binaryData = {0x01, 0x02, 0x03, 0x04};
    auto binaryMsg = Message::binary(binaryData);
    std::cout << "Binary message size: " << binaryMsg.data.size() << std::endl;

    // 关闭消息
    auto closeMsg = Message::close(CloseStatus::Normal);
    std::cout << "Close message opcode: " << static_cast<int>(closeMsg.opcode) << std::endl;

    // Ping 消息
    auto pingMsg = Message::ping();
    std::cout << "Ping message opcode: " << static_cast<int>(pingMsg.opcode) << std::endl;
}

//==============================================================================
// 示例 2: WebSocket 握手
//==============================================================================

void handshakeDemo() {
    std::cout << "\n========== 示例 2: WebSocket 握手 ==========\n" << std::endl;

    // 生成客户端 Key
    std::string clientKey = generateWebSocketKey();
    std::cout << "Client Key: " << clientKey << std::endl;

    // 计算服务端 Accept
    std::string acceptKey = Handshake::computeAcceptKey(clientKey);
    std::cout << "Server Accept: " << acceptKey << std::endl;

    // 生成客户端握手请求
    std::string request = Handshake::generateClientRequest(
        "/chat", "example.com:8080", "https://example.com", {"chat", "superchat"});
    std::cout << "\nClient Request:\n" << request << std::endl;

    // 生成服务端握手响应
    std::string response = Handshake::generateServerResponse(acceptKey, "chat");
    std::cout << "Server Response:\n" << response << std::endl;
}

//==============================================================================
// 示例 3: 帧编码和解码
//==============================================================================

void frameCodecDemo() {
    std::cout << "\n========== 示例 3: 帧编码和解码 ==========\n" << std::endl;

    // 编码文本帧 (服务器 -> 客户端, 不掩码)
    std::string text = "Hello, WebSocket!";
    auto frame = FrameParser::encodeText(text, false);
    std::cout << "Encoded frame size: " << frame.size() << " bytes" << std::endl;

    // 解码帧
    FrameParser parser;
    FrameParser::Frame decoded;
    int parsed = parser.parse(frame.data(), frame.size(), decoded);
    if (parsed > 0) {
        std::cout << "Decoded opcode: " << static_cast<int>(decoded.opcode) << std::endl;
        std::cout << "Decoded text: " << std::string(decoded.payload.begin(), decoded.payload.end()) << std::endl;
    }

    // 编码二进制帧
    std::vector<uint8_t> binaryData = {0x00, 0x01, 0x02, 0x03, 0x04};
    auto binaryFrame = FrameParser::encodeBinary(binaryData, false);
    std::cout << "\nBinary frame size: " << binaryFrame.size() << " bytes" << std::endl;

    // 编码关闭帧
    auto closeFrame = FrameParser::encodeClose(CloseStatus::Normal, "Goodbye!");
    std::cout << "Close frame size: " << closeFrame.size() << " bytes" << std::endl;
}

//==============================================================================
// 示例 4: URL 解析
//==============================================================================

void urlParsingDemo() {
    std::cout << "\n========== 示例 4: URL 解析 ==========\n" << std::endl;

    std::vector<std::string> urls = {
        "ws://localhost:8080/chat",
        "wss://example.com/socket",
        "ws://192.168.1.1:9000/path?param=value",
        "wss://api.example.com:443/v1/stream"
    };

    for (const auto& url : urls) {
        UrlInfo info = parseUrl(url);
        std::cout << "\nURL: " << url << std::endl;
        std::cout << "  SSL: " << (info.ssl ? "Yes" : "No") << std::endl;
        std::cout << "  Host: " << info.host << std::endl;
        std::cout << "  Port: " << info.port << std::endl;
        std::cout << "  Path: " << info.path << std::endl;
        std::cout << "  Query: " << info.query << std::endl;
    }
}

//==============================================================================
// 示例 5: 状态码
//==============================================================================

void statusCodeDemo() {
    std::cout << "\n========== 示例 5: WebSocket 状态码 ==========\n" << std::endl;

    std::cout << "1000 Normal Closure: "
              << static_cast<int>(CloseStatus::Normal) << std::endl;
    std::cout << "1001 Going Away: "
              << static_cast<int>(CloseStatus::GoingAway) << std::endl;
    std::cout << "1002 Protocol Error: "
              << static_cast<int>(CloseStatus::ProtocolError) << std::endl;
    std::cout << "1003 Unsupported Data: "
              << static_cast<int>(CloseStatus::UnsupportedData) << std::endl;
    std::cout << "1007 Invalid Frame Payload: "
              << static_cast<int>(CloseStatus::InvalidFramePayload) << std::endl;
    std::cout << "1008 Policy Violation: "
              << static_cast<int>(CloseStatus::PolicyViolation) << std::endl;
    std::cout << "1009 Message Too Big: "
              << static_cast<int>(CloseStatus::MessageTooBig) << std::endl;
    std::cout << "1011 Internal Error: "
              << static_cast<int>(CloseStatus::InternalError) << std::endl;
}

//==============================================================================
// 示例 6: 操作码
//==============================================================================

void opcodeDemo() {
    std::cout << "\n========== 示例 6: 操作码 ==========\n" << std::endl;

    std::cout << "0x0 Continuation: "
              << static_cast<int>(OpCode::Continuation) << std::endl;
    std::cout << "0x1 Text: "
              << static_cast<int>(OpCode::Text) << std::endl;
    std::cout << "0x2 Binary: "
              << static_cast<int>(OpCode::Binary) << std::endl;
    std::cout << "0x8 Close: "
              << static_cast<int>(OpCode::Close) << std::endl;
    std::cout << "0x9 Ping: "
              << static_cast<int>(OpCode::Ping) << std::endl;
    std::cout << "0xA Pong: "
              << static_cast<int>(OpCode::Pong) << std::endl;
}

//==============================================================================
// 示例 7: 配置选项
//==============================================================================

void configDemo() {
    std::cout << "\n========== 示例 7: 配置选项 ==========\n" << std::endl;

    Config config;
    std::cout << "Max Message Size: " << config.maxMessageSize << " bytes" << std::endl;
    std::cout << "Max Frame Size: " << config.maxFrameSize << " bytes" << std::endl;
    std::cout << "Handshake Timeout: " << config.handshakeTimeoutMs << " ms" << std::endl;
    std::cout << "Enable Compression: " << (config.enableCompression ? "Yes" : "No") << std::endl;
    std::cout << "Auto Ping: " << (config.autoPing ? "Yes" : "No") << std::endl;
    std::cout << "Ping Interval: " << config.pingIntervalMs << " ms" << std::endl;
    std::cout << "Pong Timeout: " << config.pongTimeoutMs << " ms" << std::endl;

    // 自定义配置
    Config customConfig;
    customConfig.maxMessageSize = 32 * 1024 * 1024;  // 32MB
    customConfig.autoPing = true;
    customConfig.pingIntervalMs = 15000;  // 15 seconds
    std::cout << "\nCustom Max Message Size: " << customConfig.maxMessageSize << " bytes" << std::endl;
}

//==============================================================================
// 示例 8: Base64 编解码
//==============================================================================

void base64Demo() {
    std::cout << "\n========== 示例 8: Base64 编解码 ==========\n" << std::endl;

    std::vector<uint8_t> data = {'H', 'e', 'l', 'l', 'o', ' ', 'W', 'S'};
    std::string encoded = base64Encode(data);
    std::cout << "Original: Hello WS" << std::endl;
    std::cout << "Encoded: " << encoded << std::endl;

    auto decoded = base64Decode(encoded);
    std::string decodedStr(decoded.begin(), decoded.end());
    std::cout << "Decoded: " << decodedStr << std::endl;
}

//==============================================================================
// 示例 9: SHA-1 哈希
//==============================================================================

void sha1Demo() {
    std::cout << "\n========== 示例 9: SHA-1 哈希 ==========\n" << std::endl;

    std::string input = "The quick brown fox jumps over the lazy dog";
    auto hash = sha1(input);

    std::cout << "Input: " << input << std::endl;
    std::cout << "SHA-1: ";
    for (auto byte : hash) {
        std::cout << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(byte);
    }
    std::cout << std::dec << std::endl;
}

//==============================================================================
// 主函数
//==============================================================================

int main() {
    std::cout << "==============================================" << std::endl;
    std::cout << "     Apollo WebSocket 示例" << std::endl;
    std::cout << "==============================================" << std::endl;

    try {
        messageCreationDemo();
        handshakeDemo();
        frameCodecDemo();
        urlParsingDemo();
        statusCodeDemo();
        opcodeDemo();
        configDemo();
        base64Demo();
        sha1Demo();

        std::cout << "\n==============================================" << std::endl;
        std::cout << "所有示例执行完毕!" << std::endl;
        std::cout << "==============================================" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
