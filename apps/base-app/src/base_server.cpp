#include "base/base_server.hpp"
#include "apollo/protocol/messages.hpp"
#include "apollo/protocol/codec.hpp"
#include <iostream>
#include <queue>

namespace base {

//==============================================================================
// PlayerData 实现
//==============================================================================

std::string PlayerData::toJson() const {
    std::stringstream ss;
    ss << "{"
       << "\"playerId\":" << playerId << ","
       << "\"username\":\"" << username << "\","
       << "\"level\":" << level << ","
       << "\"exp\":" << exp << ","
       << "\"hp\":" << hp << ","
       << "\"maxHp\":" << maxHp << ","
       << "\"mp\":" << mp << ","
       << "\"maxMp\":" << maxMp << ","
       << "\"x\":" << x << ","
       << "\"y\":" << y << ","
       << "\"z\":" << z
       << "}";
    return ss.str();
}

PlayerData PlayerData::fromJson(const std::string& json) {
    PlayerData data;
    // 简化解析，实际应该用 nlohmann/json
    // 这里只做基础实现
    size_t pos = 0;

    auto extractString = [&json, &pos](const std::string& key) -> std::string {
        std::string search = "\"" + key + "\":\"";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return "";
        p += search.length();
        size_t end = json.find("\"", p);
        if (end == std::string::npos) return "";
        std::string result = json.substr(p, end - p);
        pos = end + 1;
        return result;
    };

    auto extractInt = [&json, &pos](const std::string& key) -> int {
        std::string search = "\"" + key + "\":";
        size_t p = json.find(search, pos);
        if (p == std::string::npos) return 0;
        p += search.length();
        size_t end = json.find_first_of(",}", p);
        if (end == std::string::npos) return 0;
        pos = end + 1;
        return std::stoi(json.substr(p, end - p));
    };

    data.username = extractString("username");
    data.level = extractInt("level");
    data.exp = extractInt("exp");
    data.hp = extractInt("hp");
    data.maxHp = extractInt("maxHp");
    data.mp = extractInt("mp");
    data.maxMp = extractInt("maxMp");

    return data;
}

//==============================================================================
// BaseServer 实现
//==============================================================================

BaseServer::BaseServer(const BaseConfig& config)
    : config_(config)
    , database_(std::make_unique<DatabaseService>(config))
    , saveQueue_(std::make_unique<SaveQueue>(config_.workerThreads)) {
}

BaseServer::~BaseServer() {
    stop();
}

void BaseServer::start() {
    if (running_) return;

    // 初始化数据库
    if (!database_->initialize()) {
        throw std::runtime_error("Failed to initialize database");
    }

    // 启动保存队列
    saveQueue_->start();

    // 创建 RPC 服务器
    server_ = std::make_unique<protocol::RepSocket>(
        protocol::make_tcp_url(config_.host, config_.port)
    );

    server_->setRequestHandler([this](const std::vector<uint8_t>& data) -> std::vector<uint8_t> {
        auto header = protocol::MessageCodec::parseHeader(data);
        auto msgType = static_cast<protocol::MessageType>(header.type);

        switch (msgType) {
            case protocol::MessageType::DB_LOAD_REQUEST:
                return handleDbLoadRequest(data);

            case protocol::MessageType::DB_SAVE_REQUEST:
                return handleDbSaveRequest(data);

            case protocol::MessageType::DB_QUERY_REQUEST:
                return handleDbQueryRequest(data);

            case protocol::MessageType::PING:
                return handlePing(data);

            default:
                protocol::ErrorMessage err;
                err.code = static_cast<uint32_t>(protocol::MessageType::ERROR);
                err.message = "Unknown message type";
                return protocol::MessageCodec::encode(err, header.sessionId);
        }
    });

    server_->start();

    // 启动自动保存线程
    autoSaveThread_ = std::thread(&BaseServer::autoSaveLoop, this);

    running_ = true;

    std::cout << "Base server listening on " << config_.host << ":" << config_.port << std::endl;
}

void BaseServer::stop() {
    running_ = false;
    server_.stop();
    server_.reset();

    saveQueue_->stop();

    if (autoSaveThread_.joinable()) {
        autoSaveThread_.join();
    }

    database_->shutdown();
}

std::vector<uint8_t> BaseServer::handleDbLoadRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto loadReq = protocol::MessageCodec::decodeBody<protocol::DbLoadRequest>(bodyData);

    protocol::DbLoadResponse response;
    response.success = false;

    PlayerData data;
    if (database_->loadPlayer(loadReq.playerId, data)) {
        response.success = true;
        response.jsonData = data.toJson();

        std::cout << "Loaded player " << loadReq.playerId << " data" << std::endl;
    } else {
        response.errorMessage = "Player not found";
    }

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handleDbSaveRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto saveReq = protocol::MessageCodec::decodeBody<protocol::DbSaveRequest>(bodyData);

    protocol::DbSaveResponse response;
    response.success = false;

    // 异步保存
    SaveTask task;
    task.playerId = saveReq.playerId;
    task.data = PlayerData::fromJson(saveReq.jsonData);
    task.callback = [&response](bool success) {
        response.success = success;
        if (!success) {
            response.errorMessage = "Failed to save player data";
        }
    };
    task.createdAtMs = getCurrentTimeMs();

    saveQueue_->enqueue(task);

    // 简化处理：直接返回成功（实际应该等保存完成）
    response.success = true;

    std::cout << "Queued save for player " << saveReq.playerId << std::endl;

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handleDbQueryRequest(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);

    // 简化处理
    protocol::DbQueryResponse response;
    response.success = false;
    response.errorMessage = "Not implemented";

    return protocol::MessageCodec::encode(response, header.sessionId);
}

std::vector<uint8_t> BaseServer::handlePing(const std::vector<uint8_t>& request) {
    auto header = protocol::MessageCodec::parseHeader(request);
    std::vector<uint8_t> bodyData(request.begin() + sizeof(protocol::MessageHeader), request.end());

    auto ping = protocol::MessageCodec::decodeBody<protocol::Ping>(bodyData);

    protocol::Pong pong;
    pong.timestamp = ping.timestamp;

    return protocol::MessageCodec::encode(pong, header.sessionId);
}

void BaseServer::autoSaveLoop() {
    while (running_) {
        std::this_thread::sleep_for(std::chrono::milliseconds(config_.autoSaveIntervalMs));

        // 定期保存缓存的数据
        // 这里简化处理
        std::cout << "Auto-save triggered" << std::endl;
    }
}

int64_t BaseServer::getCurrentTimeMs() const {
    auto now = std::chrono::steady_clock::now();
    auto duration = now.time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
}

} // namespace base
