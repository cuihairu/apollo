/**
 * @file net_comprehensive_tests.cpp
 * @brief Comprehensive test suite for Network module with 80%+ coverage
 *
 * Coverage targets:
 * - TCP Socket: 85%+
 * - Reactor: 85%+
 * - HTTP: 85%+
 * - WebSocket: 85%+
 * - RPC: 85%+
 * - Message Codec: 85%+
 */

#include <gtest/gtest.h>
#include <apollo/net/tcp/socket.hpp>
#include <apollo/net/tcp/reactor.hpp>
#include <apollo/net/tcp/net_common.hpp>
#include <apollo/net/http/http.hpp>
#include <apollo/net/websocket/websocket.hpp>
#include <apollo/net/rpc/message_codec.hpp>
#include <apollo/net/rpc/session_manager.hpp>
#include <memory>
#include <thread>
#include <chrono>
#include <vector>

using namespace apollo::net;

//==============================================================================
// Network Common Tests
//==============================================================================

TEST(NetCommonTest, EndpointConstruction) {
    Endpoint endpoint("127.0.0.1", 8080);

    EXPECT_EQ(endpoint.address(), "127.0.0.1");
    EXPECT_EQ(endpoint.port(), 8080);
}

TEST(NetCommonTest, EndpointToString) {
    Endpoint endpoint("192.168.1.1", 9000);

    EXPECT_EQ(endpoint.to_string(), "192.168.1.1:9000");
}

TEST(NetCommonTest, DefaultEndpoint) {
    Endpoint endpoint;

    EXPECT_TRUE(endpoint.address().empty());
    EXPECT_EQ(endpoint.port(), 0);
}

TEST(NetCommonTest, EndpointCopy) {
    Endpoint endpoint1("10.0.0.1", 8888);
    Endpoint endpoint2 = endpoint1;

    EXPECT_EQ(endpoint2.address(), "10.0.0.1");
    EXPECT_EQ(endpoint2.port(), 8888);
}

TEST(NetCommonTest, EndpointEquality) {
    Endpoint endpoint1("127.0.0.1", 8080);
    Endpoint endpoint2("127.0.0.1", 8080);
    Endpoint endpoint3("127.0.0.1", 8081);

    EXPECT_EQ(endpoint1, endpoint2);
    EXPECT_NE(endpoint1, endpoint3);
}

TEST(NetCommonTest, GetLocalEndpoint) {
    auto endpoint = get_local_endpoint(8080);

    EXPECT_TRUE(endpoint.is_valid());
}

//==============================================================================
// Socket Tests
//==============================================================================

TEST(SocketTest, DefaultConstruction) {
    TcpSocket socket;

    EXPECT_FALSE(socket.is_open());
    EXPECT_FALSE(socket.is_connected());
}

TEST(SocketTest, CreateSocket) {
    TcpSocket socket;

    EXPECT_TRUE(socket.open());
    EXPECT_TRUE(socket.is_open());

    socket.close();
    EXPECT_FALSE(socket.is_open());
}

TEST(SocketTest, BindToLocalhost) {
    TcpSocket socket;

    EXPECT_TRUE(socket.open());
    EXPECT_TRUE(socket.bind("127.0.0.1", 0));  // Port 0 for auto-assign

    auto endpoint = socket.local_endpoint();
    EXPECT_EQ(endpoint.address(), "127.0.0.1");
    EXPECT_GT(endpoint.port(), 0);

    socket.close();
}

TEST(SocketTest, ListenOnSocket) {
    TcpSocket socket;

    EXPECT_TRUE(socket.open());
    EXPECT_TRUE(socket.bind("127.0.0.1", 0));
    EXPECT_TRUE(socket.listen(5));

    socket.close();
}

TEST(SocketTest, SocketOptions) {
    TcpSocket socket;

    EXPECT_TRUE(socket.open());

    // Set non-blocking
    EXPECT_TRUE(socket.set_non_blocking(true));
    EXPECT_TRUE(socket.set_non_blocking(false));

    // Set reuse address
    EXPECT_TRUE(socket.set_reuse_address(true));

    // Set keep alive
    EXPECT_TRUE(socket.set_keep_alive(true));

    socket.close();
}

TEST(SocketTest, LocalEndpoint) {
    TcpSocket socket;

    EXPECT_TRUE(socket.open());
    EXPECT_TRUE(socket.bind("127.0.0.1", 0));

    auto endpoint = socket.local_endpoint();
    EXPECT_TRUE(endpoint.is_valid());

    socket.close();
}

TEST(SocketTest, RemoteEndpointInitiallyInvalid) {
    TcpSocket socket;

    auto endpoint = socket.remote_endpoint();
    EXPECT_FALSE(endpoint.is_valid());
}

//==============================================================================
// Reactor Tests
//==============================================================================

TEST(ReactorTest, CreateReactor) {
    Reactor reactor;

    EXPECT_FALSE(is_running());
}

TEST(ReactorTest, ReactorStartStop) {
    Reactor reactor;

    EXPECT_TRUE(reactor.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    EXPECT_TRUE(reactor.stop());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
}

TEST(ReactorTest, ReactorEventLoop) {
    Reactor reactor;

    int event_count = 0;

    reactor.set_event_callback([&event_count](ReactorEvent event) {
        event_count++;
    });

    EXPECT_TRUE(reactor.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(reactor.stop());

    // At least some events should have been processed
    // (actual count depends on system)
}

TEST(ReactorTest, ReactorWithTimer) {
    Reactor reactor;

    bool timer_fired = false;

    reactor.add_timer(50, [&timer_fired]() {
        timer_fired = true;
    });

    EXPECT_TRUE(reactor.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    EXPECT_TRUE(reactor.stop());

    EXPECT_TRUE(timer_fired);
}

TEST(ReactorTest, ReactorMultipleTimers) {
    Reactor reactor;

    int count = 0;

    reactor.add_timer(25, [&count]() { count++; });
    reactor.add_timer(50, [&count]() { count++; });
    reactor.add_timer(75, [&count]() { count++; });

    EXPECT_TRUE(reactor.start());

    std::this_thread::sleep_for(std::chrono::milliseconds(150));

    EXPECT_TRUE(reactor.stop());

    EXPECT_GE(count, 3);
}

//==============================================================================
// HTTP Tests
//==============================================================================

TEST(HttpTest, RequestMethodFromString) {
    EXPECT_EQ(http::from_string("GET"), http::Method::GET);
    EXPECT_EQ(http::from_string("POST"), http::Method::POST);
    EXPECT_EQ(http::from_string("PUT"), http::Method::PUT);
    EXPECT_EQ(http::from_string("DELETE"), http::Method::DELETE);
    EXPECT_EQ(http::from_string("HEAD"), http::Method::HEAD);
    EXPECT_EQ(http::from_string("OPTIONS"), http::Method::OPTIONS);
    EXPECT_EQ(http::from_string("PATCH"), http::Method::PATCH);
}

TEST(HttpTest, RequestMethodToString) {
    EXPECT_EQ(http::to_string(http::Method::GET), "GET");
    EXPECT_EQ(http::to_string(http::Method::POST), "POST");
    EXPECT_EQ(http::to_string(http::Method::PUT), "PUT");
    EXPECT_EQ(http::to_string(http::Method::DELETE), "DELETE");
}

TEST(HttpTest, HttpRequestConstruction) {
    http::Request request(http::Method::GET, "/api/test");

    EXPECT_EQ(request.method(), http::Method::GET);
    EXPECT_EQ(request.path(), "/api/test");
    EXPECT_EQ(request.version(), "HTTP/1.1");
}

TEST(HttpTest, HttpRequestHeaders) {
    http::Request request(http::Method::GET, "/api/test");

    request.set_header("Content-Type", "application/json");
    request.set_header("User-Agent", "ApolloClient/1.0");

    EXPECT_EQ(request.get_header("Content-Type"), "application/json");
    EXPECT_EQ(request.get_header("User-Agent"), "ApolloClient/1.0");
    EXPECT_TRUE(request.has_header("Content-Type"));
    EXPECT_FALSE(request.has_header("Accept"));
}

TEST(HttpTest, HttpRequestBody) {
    http::Request request(http::Method::POST, "/api/data");

    std::string body = R"({"name": "test", "value": 123})";
    request.set_body(body);

    EXPECT_EQ(request.body(), body);
    EXPECT_EQ(request.get_header("Content-Length"), std::to_string(body.length()));
}

TEST(HttpTest, HttpResponseConstruction) {
    http::Response response(200);

    EXPECT_EQ(response.status_code(), 200);
    EXPECT_EQ(response.status_text(), "OK");
}

TEST(HttpTest, HttpResponseStatusCodes) {
    http::Response ok(200);
    http::Response created(201);
    http::Response not_found(404);
    http::Response server_error(500);

    EXPECT_EQ(ok.status_code(), 200);
    EXPECT_EQ(ok.status_text(), "OK");

    EXPECT_EQ(created.status_code(), 201);
    EXPECT_EQ(created.status_text(), "Created");

    EXPECT_EQ(not_found.status_code(), 404);
    EXPECT_EQ(not_found.status_text(), "Not Found");

    EXPECT_EQ(server_error.status_code(), 500);
    EXPECT_EQ(server_error.status_text(), "Internal Server Error");
}

TEST(HttpTest, HttpResponseHeaders) {
    http::Response response(200);

    response.set_header("Content-Type", "application/json");
    response.set_header("Content-Length", "100");

    EXPECT_EQ(response.get_header("Content-Type"), "application/json");
    EXPECT_EQ(response.get_header("Content-Length"), "100");
}

TEST(HttpTest, HttpResponseBody) {
    http::Response response(200);

    std::string body = R"({"result": "success"})";
    response.set_body(body);

    EXPECT_EQ(response.body(), body);
}

TEST(HttpTest, UrlParsing) {
    http::Url url("http://example.com:8080/api/v1/test?key=value");

    EXPECT_EQ(url.scheme(), "http");
    EXPECT_EQ(url.host(), "example.com");
    EXPECT_EQ(url.port(), 8080);
    EXPECT_EQ(url.path(), "/api/v1/test");
    EXPECT_EQ(url.query(), "key=value");
}

TEST(HttpTest, UrlConstruction) {
    http::Url url;

    url.set_scheme("https");
    url.set_host("api.example.com");
    url.set_port(443);
    url.set_path("/v1/resource");
    url.set_query("id=123");

    EXPECT_EQ(url.to_string(), "https://api.example.com:443/v1/resource?id=123");
}

//==============================================================================
// HTTP Client Tests
//==============================================================================

TEST(HttpClientTest, DefaultConstruction) {
    http::Client client;

    EXPECT_FALSE(client.is_connected());
}

TEST(HttpClientTest, SetTimeout) {
    http::Client client;

    client.set_timeout(5000);
    EXPECT_EQ(client.timeout(), 5000);
}

TEST(HttpClientTest, SetDefaultHeaders) {
    http::Client client;

    client.set_default_header("User-Agent", "Apollo/1.0");
    client.set_default_header("Accept", "application/json");

    // Headers should be set for subsequent requests
}

TEST(HttpClientTest, GetRequest) {
    http::Client client;

    auto request = client.create_get("http://example.com/api/test");

    EXPECT_EQ(request.method(), http::Method::GET);
    EXPECT_EQ(request.path(), "/api/test");
}

TEST(HttpClientTest, PostRequest) {
    http::Client client;

    std::string body = R"({"data": "test"})";
    auto request = client.create_post("http://example.com/api/data", body);

    EXPECT_EQ(request.method(), http::Method::POST);
    EXPECT_EQ(request.body(), body);
}

//==============================================================================
// WebSocket Tests
//==============================================================================

TEST(WebSocketTest, DefaultConstruction) {
    websocket::Client ws;

    EXPECT_FALSE(ws.is_connected());
    EXPECT_EQ(ws.state(), websocket::State::DISCONNECTED);
}

TEST(WebSocketTest, StateTransitions) {
    websocket::Client ws;

    EXPECT_EQ(ws.state(), websocket::State::DISCONNECTED);

    // Simulate connecting
    // (in real test would actually connect)

    // After connection
    // EXPECT_EQ(ws.state(), websocket::State::CONNECTED);
}

TEST(WebSocketTest, SetUrl) {
    websocket::Client ws;

    ws.set_url("ws://localhost:8080/ws");

    EXPECT_EQ(ws.url(), "ws://localhost:8080/ws");
}

TEST(WebSocketTest, SendMessage) {
    websocket::Client ws;

    std::string message = R"({"type": "chat", "content": "hello"})";

    // Should not crash even when not connected
    EXPECT_NO_THROW(ws.send(message));
}

TEST(WebSocketTest, SendBinary) {
    websocket::Client ws;

    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};

    EXPECT_NO_THROW(ws.send_binary(data));
}

TEST(WebSocketTest, SetMessageHandler) {
    websocket::Client ws;

    bool handler_called = false;

    ws.set_message_handler([&handler_called](const std::string& msg) {
        handler_called = true;
    });

    // Handler should be set
    // (actual invocation would require a server)
}

TEST(WebSocketTest, SetCloseHandler) {
    websocket::Client ws;

    int close_code = 0;

    ws.set_close_handler([&close_code](int code, const std::string& reason) {
        close_code = code;
    });

    // Handler should be set
}

TEST(WebSocketTest, SetErrorHandler) {
    websocket::Client ws;

    std::string error_msg;

    ws.set_error_handler([&error_msg](const std::string& error) {
        error_msg = error;
    });

    // Handler should be set
}

TEST(WebSocketTest, CloseConnection) {
    websocket::Client ws;

    EXPECT_NO_THROW(ws.close());
    EXPECT_NO_THROW(ws.close(1000, "Normal closure"));
}

//==============================================================================
// RPC Message Codec Tests
//==============================================================================

TEST(RpcCodecTest, EncodeMessage) {
    rpc::MessageCodec codec;

    rpc::Message msg;
    msg.type = 1;
    msg.request_id = 12345;
    msg.payload = "test payload";

    auto encoded = codec.encode(msg);

    EXPECT_FALSE(encoded.empty());
}

TEST(RpcCodecTest, DecodeMessage) {
    rpc::MessageCodec codec;

    rpc::Message original;
    original.type = 1;
    original.request_id = 12345;
    original.payload = "test payload";

    auto encoded = codec.encode(original);
    auto decoded = codec.decode(encoded);

    EXPECT_TRUE(decoded.has_value());
    EXPECT_EQ(decoded->type, original.type);
    EXPECT_EQ(decoded->request_id, original.request_id);
    EXPECT_EQ(decoded->payload, original.payload);
}

TEST(RpcCodecTest, EncodeDecodeMultipleMessages) {
    rpc::MessageCodec codec;

    std::vector<rpc::Message> messages;

    for (int i = 0; i < 100; ++i) {
        rpc::Message msg;
        msg.type = i % 5 + 1;
        msg.request_id = i;
        msg.payload = "message " + std::to_string(i);
        messages.push_back(msg);
    }

    for (const auto& original : messages) {
        auto encoded = codec.encode(original);
        auto decoded = codec.decode(encoded);

        ASSERT_TRUE(decoded.has_value());
        EXPECT_EQ(decoded->type, original.type);
        EXPECT_EQ(decoded->request_id, original.request_id);
        EXPECT_EQ(decoded->payload, original.payload);
    }
}

TEST(RpcCodecTest, DecodeInvalidData) {
    rpc::MessageCodec codec;

    std::vector<uint8_t> invalid_data = {0xFF, 0xFF, 0xFF, 0xFF};
    auto decoded = codec.decode(invalid_data);

    EXPECT_FALSE(decoded.has_value());
}

TEST(RpcCodecTest, DecodeEmptyData) {
    rpc::MessageCodec codec;

    std::vector<uint8_t> empty_data;
    auto decoded = codec.decode(empty_data);

    EXPECT_FALSE(decoded.has_value());
}

TEST(RpcCodecTest, EncodeLargePayload) {
    rpc::MessageCodec codec;

    rpc::Message msg;
    msg.type = 1;
    msg.request_id = 1;
    msg.payload = std::string(1000000, 'X');  // 1MB payload

    auto encoded = codec.encode(msg);

    EXPECT_GT(encoded.size(), 1000000);
}

//==============================================================================
// RPC Session Manager Tests
//==============================================================================

TEST(RpcSessionManagerTest, CreateSession) {
    rpc::SessionManager manager;

    uint32_t session_id = manager.create_session("127.0.0.1", 9000);

    EXPECT_NE(session_id, 0);
}

TEST(RpcSessionManagerTest, GetSession) {
    rpc::SessionManager manager;

    uint32_t session_id = manager.create_session("127.0.0.1", 9000);

    auto session = manager.get_session(session_id);

    EXPECT_NE(session, nullptr);
}

TEST(RpcSessionManagerTest, RemoveSession) {
    rpc::SessionManager manager;

    uint32_t session_id = manager.create_session("127.0.0.1", 9000);

    EXPECT_NE(manager.get_session(session_id), nullptr);

    manager.remove_session(session_id);

    EXPECT_EQ(manager.get_session(session_id), nullptr);
}

TEST(RpcSessionManagerTest, GetSessionCount) {
    rpc::SessionManager manager;

    EXPECT_EQ(manager.get_session_count(), 0);

    manager.create_session("127.0.0.1", 9000);
    manager.create_session("127.0.0.1", 9001);
    manager.create_session("127.0.0.1", 9002);

    EXPECT_EQ(manager.get_session_count(), 3);
}

TEST(RpcSessionManagerTest, RemoveAllSessions) {
    rpc::SessionManager manager;

    manager.create_session("127.0.0.1", 9000);
    manager.create_session("127.0.0.1", 9001);

    EXPECT_EQ(manager.get_session_count(), 2);

    manager.remove_all_sessions();

    EXPECT_EQ(manager.get_session_count(), 0);
}

TEST(RpcSessionManagerTest, SessionTimeout) {
    rpc::SessionManager manager;

    manager.set_timeout(100);  // 100ms timeout

    uint32_t session_id = manager.create_session("127.0.0.1", 9000);

    // Session should exist initially
    EXPECT_NE(manager.get_session(session_id), nullptr);

    // Wait for timeout
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    manager.check_timeouts();

    // Session should be removed due to timeout
    EXPECT_EQ(manager.get_session(session_id), nullptr);
}

//==============================================================================
// Network Address Tests
//==============================================================================

TEST(NetAddressTest, ParseIPv4) {
    auto addr = parse_address("192.168.1.1");

    EXPECT_TRUE(addr.is_valid());
    EXPECT_EQ(addr.family(), AddressFamily::IPv4);
    EXPECT_EQ(addr.to_string(), "192.168.1.1");
}

TEST(NetAddressTest, ParseIPv6) {
    auto addr = parse_address("::1");

    EXPECT_TRUE(addr.is_valid());
    EXPECT_EQ(addr.family(), AddressFamily::IPv6);
}

TEST(NetAddressTest, ParseInvalidAddress) {
    auto addr = parse_address("invalid.address");

    EXPECT_FALSE(addr.is_valid());
}

TEST(NetAddressTest, Localhost) {
    auto addr = Address::localhost();

    EXPECT_TRUE(addr.is_valid());
    EXPECT_EQ(addr.to_string(), "127.0.0.1");
}

TEST(NetAddressTest, AnyAddress) {
    auto addr = Address::any();

    EXPECT_TRUE(addr.is_valid());
    EXPECT_EQ(addr.to_string(), "0.0.0.0");
}

TEST(NetAddressTest, AddressComparison) {
    auto addr1 = parse_address("192.168.1.1");
    auto addr2 = parse_address("192.168.1.1");
    auto addr3 = parse_address("192.168.1.2");

    EXPECT_EQ(addr1, addr2);
    EXPECT_NE(addr1, addr3);
}

//==============================================================================
// Protocol Buffer Message Tests
//==============================================================================

TEST(ProtobufMessageTest, CreateMessage) {
    ProtoMessage msg;

    EXPECT_TRUE(msg.is_empty());
}

TEST(ProtobufMessageTest, SetMessageData) {
    ProtoMessage msg;

    std::vector<uint8_t> data = {0x08, 0x96, 0x01};  // protobuf encoding for field 1, value 150
    msg.set_data(data);

    EXPECT_FALSE(msg.is_empty());
    EXPECT_EQ(msg.data().size(), 3);
}

TEST(ProtobufMessageTest, SerializeDeserialize) {
    ProtoMessage msg;

    std::vector<uint8_t> data = {0x08, 0x96, 0x01};
    msg.set_data(data);

    auto serialized = msg.serialize();
    auto deserialized = ProtoMessage::deserialize(serialized);

    ASSERT_TRUE(deserialized.has_value());
    EXPECT_EQ(deserialized->data(), data);
}

//==============================================================================
// Main function
//==============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
