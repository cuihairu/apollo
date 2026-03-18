#pragma once

#include "base/config.hpp"
#include "base/database_service.hpp"
#include "apollo/protocol/socket.hpp"
#include <memory>
#include <thread>
#include <atomic>

namespace base {

// BaseApp 服务器
class BaseServer {
public:
    explicit BaseServer(const BaseConfig& config);
    ~BaseServer();

    // 启动服务器
    void start();

    // 停止服务器
    void stop();

    // 是否运行中
    bool isRunning() const { return running_; }

private:
    // 处理数据库加载请求
    std::vector<uint8_t> handleDbLoadRequest(const std::vector<uint8_t>& request);

    // 处理数据库保存请求
    std::vector<uint8_t> handleDbSaveRequest(const std::vector<uint8_t>& request);

    // 处理查询请求
    std::vector<uint8_t> handleDbQueryRequest(const std::vector<uint8_t>& request);

    // 处理心跳
    std::vector<uint8_t> handlePing(const std::vector<uint8_t>& request);

    // 自动保存循环
    void autoSaveLoop();

    BaseConfig config_;
    std::unique_ptr<DatabaseService> database_;
    std::unique_ptr<SaveQueue> saveQueue_;

    std::unique_ptr<protocol::RepSocket> server_;
    std::atomic<bool> running_{false};

    std::thread autoSaveThread_;
};

} // namespace base
