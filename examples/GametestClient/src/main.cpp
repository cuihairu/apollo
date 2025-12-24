#include "gametest_client.h"
#include "shell.h"
#include <iostream>
#include <memory>
#include <signal.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace gametest;

std::shared_ptr<Shell> g_shell;
std::shared_ptr<GametestClient> g_client;

void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在退出...\n";
    if (g_shell) {
        g_shell->stop();
    }
    if (g_client) {
        g_client->stop();
    }
    exit(0);
}

void messageHandler(const json& message) {
    std::cout << "\n[服务器消息] " << message.dump(2) << std::endl;
    // 重新显示提示符
    std::cout << "gametest> ";
    std::flush(std::cout);
}

void logHandler(const std::string& message) {
    std::cout << "[日志] " << message << std::endl;
}

int main(int argc, char* argv[]) {
    // 设置信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    // 创建客户端
    g_client = std::make_shared<GametestClient>();
    g_client->setLogHandler(logHandler);
    g_client->setMessageHandler(messageHandler);

    // 创建 Shell
    g_shell = std::make_shared<Shell>(g_client);

    std::cout << "Gametest Client 启动中...\n";

    // 如果提供了命令行参数，尝试自动登录
    if (argc > 1) {
        std::string configFile = argv[1];
        std::cout << "使用配置文件: " << configFile << std::endl;

        if (g_client->autoLogin(configFile)) {
            std::cout << "自动登录成功！\n";
        } else {
            std::cout << "自动登录失败，请手动连接\n";
        }
    }

    // 运行 Shell
    g_shell->run();

    std::cout << "Gametest Client 已退出\n";
    return 0;
}