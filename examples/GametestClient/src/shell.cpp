#include "shell.h"
#include "gametest_client.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <vector>
#include <string>
#include <cstring>
#include <cstdlib>
#include <readline/readline.h>
#include <readline/history.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace gametest {

// 全局 Shell 实例指针（用于 readline 回调）
static Shell* g_shell_instance = nullptr;

// Readline 自动补全回调函数
static char* command_generator(const char* text, int state) {
    static std::vector<std::string> matches;
    static size_t match_index = 0;

    if (state == 0) {
        match_index = 0;
        matches.clear();

        if (!g_shell_instance) {
            return nullptr;
        }

        // 获取所有命令名称
        for (const auto& cmd : g_shell_instance->getCommands()) {
            const std::string& name = cmd.first;
            if (name.find(text) == 0) {
                matches.push_back(name);
            }
        }

        // 获取历史命令
        HIST_ENTRY** hist_list = history_list();
        if (hist_list) {
            for (int i = 0; hist_list[i]; ++i) {
                std::string hist_entry = hist_list[i]->line;
                if (hist_entry.find(text) == 0) {
                    // 避免重复添加命令
                    if (g_shell_instance->getCommands().find(hist_entry) == g_shell_instance->getCommands().end()) {
                        matches.push_back(hist_entry);
                    }
                }
            }
        }

        // 去重
        std::sort(matches.begin(), matches.end());
        matches.erase(std::unique(matches.begin(), matches.end()), matches.end());
    }

    if (match_index < matches.size()) {
        return strdup(matches[match_index++].c_str());
    }

    return nullptr;
}

static char** command_completion(const char* text, int start, int end) {
    char** matches = nullptr;

    if (start == 0) {
        // 正在补全命令
        rl_attempted_completion_over = 1;
        matches = rl_completion_matches(text, command_generator);
    } else {
        // 参数补全
        std::string line = rl_line_buffer;
        std::istringstream iss(line);
        std::string cmd;
        std::vector<std::string> args;

        if (iss >> cmd) {
            std::string arg;
            while (iss >> arg) {
                args.push_back(arg);
            }
        }

        // 根据不同命令提供参数补全
        if (cmd == "connect" && args.size() == 0) {
            // 补全主机名
            static const char* hosts[] = {"localhost", "127.0.0.1", "192.168.1.100", "10.0.0.1", nullptr};
            rl_attempted_completion_over = 1;
            matches = rl_completion_matches(text, [](const char* text, int state) -> char* {
                static int index = 0;
                if (state == 0) {
                    index = 0;
                }
                while (hosts[index]) {
                    if (strstr(hosts[index], text)) {
                        return strdup(hosts[index++]);
                    }
                    index++;
                }
                return nullptr;
            });
        } else if (cmd == "connect" && args.size() == 1) {
            // 补全端口号
            static const char* ports[] = {"8080", "3000", "80", "443", "22", "21", nullptr};
            rl_attempted_completion_over = 1;
            matches = rl_completion_matches(text, [](const char* text, int state) -> char* {
                static int index = 0;
                if (state == 0) {
                    index = 0;
                }
                while (ports[index]) {
                    if (strstr(ports[index], text)) {
                        return strdup(ports[index++]);
                    }
                    index++;
                }
                return nullptr;
            });
        } else if (cmd == "autologin" && args.size() == 0) {
            // 补全配置文件路径
            static const char* configs[] = {"config/login.json", "config/dev.json", "config/prod.json", nullptr};
            rl_attempted_completion_over = 1;
            matches = rl_completion_matches(text, [](const char* text, int state) -> char* {
                static int index = 0;
                if (state == 0) {
                    index = 0;
                }
                while (configs[index]) {
                    if (strstr(configs[index], text)) {
                        return strdup(configs[index++]);
                    }
                    index++;
                }
                return nullptr;
            });
        }
    }

    return matches;
}

Shell::Shell(std::shared_ptr<GametestClient> client)
    : client_(client)
    , prompt_("gametest> ")
    , running_(false) {

    // 设置全局实例指针
    g_shell_instance = this;

    // 注册内建命令
    registerCommand({
        "help",
        "显示帮助信息",
        [this](const std::vector<std::string>& args) { cmdHelp(args); },
        "help [command]"
    });

    registerCommand({
        "quit",
        "退出程序",
        [this](const std::vector<std::string>& args) { cmdQuit(args); },
        "quit"
    });

    registerCommand({
        "exit",
        "退出程序",
        [this](const std::vector<std::string>& args) { cmdQuit(args); },
        "exit"
    });

    registerCommand({
        "connect",
        "连接到服务器",
        [this](const std::vector<std::string>& args) { cmdConnect(args); },
        "connect <host> <port>"
    });

    registerCommand({
        "disconnect",
        "断开连接",
        [this](const std::vector<std::string>& args) { cmdDisconnect(args); },
        "disconnect"
    });

    registerCommand({
        "login",
        "用户登录",
        [this](const std::vector<std::string>& args) { cmdLogin(args); },
        "login <username> <password>"
    });

    registerCommand({
        "autologin",
        "使用配置文件自动登录",
        [this](const std::vector<std::string>& args) { cmdAutoLogin(args); },
        "autologin [config_file]"
    });

    registerCommand({
        "send",
        "发送 JSON 消息到服务器",
        [this](const std::vector<std::string>& args) { cmdSend(args); },
        "send <json_message>"
    });

    registerCommand({
        "status",
        "显示当前连接状态",
        [this](const std::vector<std::string>& args) { cmdStatus(args); },
        "status"
    });

    registerCommand({
        "info",
        "显示服务器信息",
        [this](const std::vector<std::string>& args) { cmdInfo(args); },
        "info"
    });

    registerCommand({
        "clear",
        "清屏",
        [this](const std::vector<std::string>& args) { cmdClear(args); },
        "clear"
    });

    // 配置 readline
    rl_attempted_completion_function = command_completion;
    rl_bind_key('\t', rl_complete);

    // 设置历史记录
    using_history();
    stifle_history(1000); // 最多保存1000条历史

    // 设置历史文件路径
#ifdef _WIN32
    const char* home = getenv("USERPROFILE");
#else
    const char* home = getenv("HOME");
#endif
    history_file_ = std::string(home ? home : ".") + "/.gametest_history";

    // 读取历史文件
    read_history(history_file_.c_str());

    // 设置智能提示
    rl_variable_bind("show-all-if-ambiguous", "on");
    rl_variable_bind("completion-ignore-case", "on");
}

Shell::~Shell() {
    // 保存历史记录
    write_history(history_file_.c_str());
    clear_history();

    stop();
    g_shell_instance = nullptr;
}

void Shell::run() {
    running_ = true;
    printWelcome();

    char* line = nullptr;
    while (running_) {
        line = readline(prompt_.c_str());

        if (!line) { // EOF (Ctrl+D)
            std::cout << "\n";
            break;
        }

        std::string lineStr(line);
        free(line);

        // 跳过空行
        if (lineStr.empty()) {
            continue;
        }

        // 不保存重复命令到历史
        if (history_length > 0 && strcmp(lineStr.c_str(), history_get(history_length)->line) == 0) {
            // 命令重复，不添加到历史
        } else {
            // 添加到历史
            add_history(lineStr.c_str());
        }

        // 处理命令
        processCommand(lineStr);
    }
}

void Shell::stop() {
    running_ = false;
}

void Shell::registerCommand(const Command& command) {
    commands_[command.name] = command;
}

void Shell::processCommand(const std::string& line) {
    std::string cmd;
    std::vector<std::string> args;

    parseCommand(line, cmd, args);

    if (cmd.empty()) {
        return;
    }

    auto it = commands_.find(cmd);
    if (it != commands_.end()) {
        try {
            it->second.handler(args);
        } catch (const std::exception& e) {
            std::cerr << "错误: " << e.what() << std::endl;
        }
    } else {
        std::cerr << "未知命令: " << cmd << std::endl;
        std::cerr << "输入 'help' 查看可用命令" << std::endl;

        // 提供相似命令建议
        std::vector<std::pair<std::string, int>> distances;
        for (const auto& pair : commands_) {
            int dist = levenshtein_distance(cmd, pair.first);
            if (dist <= 2) {
                distances.push_back({pair.first, dist});
            }
        }

        if (!distances.empty()) {
            std::sort(distances.begin(), distances.end(),
                [](const auto& a, const auto& b) { return a.second < b.second; });

            std::cerr << "您是否想输入: ";
            for (size_t i = 0; i < distances.size() && i < 3; ++i) {
                if (i > 0) std::cerr << " 或 ";
                std::cerr << distances[i].first;
            }
            std::cerr << " ?" << std::endl;
        }
    }
}

void Shell::parseCommand(const std::string& line, std::string& cmd, std::vector<std::string>& args) {
    std::istringstream iss(line);
    std::string token;

    if (iss >> cmd) {
        args = splitArgs(line.substr(cmd.length()));
    }
}

std::vector<std::string> Shell::splitArgs(const std::string& args) {
    std::vector<std::string> result;
    std::istringstream iss(args);
    std::string token;

    while (iss >> token) {
        // 处理带引号的字符串
        if (token.front() == '"' && token.back() == '"') {
            result.push_back(token.substr(1, token.length() - 2));
        } else if (token.front() == '\'') {
            result.push_back(token.substr(1, token.length() - 1));
        } else {
            result.push_back(token);
        }
    }

    return result;
}

int Shell::levenshtein_distance(const std::string& s1, const std::string& s2) {
    const int m = s1.length();
    const int n = s2.length();
    std::vector<std::vector<int>> dp(m + 1, std::vector<int>(n + 1));

    for (int i = 0; i <= m; ++i) dp[i][0] = i;
    for (int j = 0; j <= n; ++j) dp[0][j] = j;

    for (int i = 1; i <= m; ++i) {
        for (int j = 1; j <= n; ++j) {
            if (s1[i-1] == s2[j-1]) {
                dp[i][j] = dp[i-1][j-1];
            } else {
                dp[i][j] = 1 + std::min({dp[i-1][j], dp[i][j-1], dp[i-1][j-1]});
            }
        }
    }

    return dp[m][n];
}

void Shell::cmdHelp(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cout << "可用命令:\n";
        for (const auto& pair : commands_) {
            const auto& cmd = pair.second;
            std::cout << "  " << std::left << std::setw(12) << cmd.name
                      << " - " << cmd.description << "\n";
        }
        std::cout << "\n输入 'help <command>' 查看命令详细用法\n";
        std::cout << "提示: 使用 Tab 键进行命令自动补全\n";
    } else {
        auto it = commands_.find(args[0]);
        if (it != commands_.end()) {
            const auto& cmd = it->second;
            std::cout << "命令: " << cmd.name << "\n";
            std::cout << "描述: " << cmd.description << "\n";
            std::cout << "用法: " << cmd.usage << "\n";

            // 显示相关命令
            std::vector<std::string> related;
            for (const auto& pair : commands_) {
                if (pair.first.find(args[0]) != std::string::npos && pair.first != args[0]) {
                    related.push_back(pair.first);
                }
            }
            if (!related.empty()) {
                std::cout << "相关命令: ";
                for (size_t i = 0; i < related.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << related[i];
                }
                std::cout << "\n";
            }
        } else {
            std::cerr << "未知命令: " << args[0] << "\n";
        }
    }
}

void Shell::cmdQuit(const std::vector<std::string>& args) {
    std::cout << "正在退出...\n";
    running_ = false;
    client_->stop();
}

void Shell::cmdConnect(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "用法: " << commands_["connect"].usage << "\n";
        std::cerr << "提示: 使用 Tab 键可以自动补全主机名和端口号\n";
        return;
    }

    std::string host = args[0];
    int port = std::stoi(args[1]);

    std::cout << "正在连接到 " << host << ":" << port << "...\n";

    if (client_->connect(host, port)) {
        std::cout << "连接成功!\n";
        // 添加到最近连接历史
        recent_hosts_.push_back(host);
        if (recent_hosts_.size() > 5) {
            recent_hosts_.erase(recent_hosts_.begin());
        }
    } else {
        std::cerr << "连接失败!\n";
    }
}

void Shell::cmdDisconnect(const std::vector<std::string>& args) {
    std::cout << "正在断开连接...\n";
    client_->disconnect();
}

void Shell::cmdLogin(const std::vector<std::string>& args) {
    if (args.size() < 2) {
        std::cerr << "用法: " << commands_["login"].usage << "\n";
        return;
    }

    std::string username = args[0];
    std::string password = args[1];

    std::cout << "正在登录...\n";

    if (client_->login(username, password)) {
        std::cout << "登录成功!\n";
    } else {
        std::cerr << "登录失败!\n";
    }
}

void Shell::cmdAutoLogin(const std::vector<std::string>& args) {
    std::string configFile = "config/login.json";
    if (!args.empty()) {
        configFile = args[0];
    }

    std::cout << "正在使用配置文件自动登录: " << configFile << "\n";

    if (client_->autoLogin(configFile)) {
        std::cout << "自动登录成功!\n";
    } else {
        std::cerr << "自动登录失败!\n";
    }
}

void Shell::cmdSend(const std::vector<std::string>& args) {
    if (args.empty()) {
        std::cerr << "用法: " << commands_["send"].usage << "\n";
        std::cerr << "示例: send {\"type\":\"ping\"}\n";
        return;
    }

    // 将参数组合成 JSON 字符串
    std::string jsonStr;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i > 0) jsonStr += " ";
        jsonStr += args[i];
    }

    try {
        json message = json::parse(jsonStr);
        client_->sendMessage(message);
        std::cout << "消息已发送: " << message.dump(2) << "\n";
    } catch (const json::exception& e) {
        std::cerr << "无效的 JSON 格式: " << e.what() << "\n";
        std::cerr << "提示: JSON 字符串需要用引号包围\n";
    }
}

void Shell::cmdStatus(const std::vector<std::string>& args) {
    auto state = client_->getState();
    std::string stateStr;
    std::string stateColor;

    switch (state) {
        case gametest::ClientState::DISCONNECTED:
            stateStr = "未连接";
            stateColor = "\033[31m"; // 红色
            break;
        case gametest::ClientState::CONNECTING:
            stateStr = "连接中";
            stateColor = "\033[33m"; // 黄色
            break;
        case gametest::ClientState::CONNECTED:
            stateStr = "已连接";
            stateColor = "\033[32m"; // 绿色
            break;
        case gametest::ClientState::AUTHENTICATED:
            stateStr = "已认证";
            stateColor = "\033[32m"; // 绿色
            break;
        case gametest::ClientState::ERROR:
            stateStr = "错误";
            stateColor = "\033[31m"; // 红色
            break;
    }

    std::cout << "当前状态: " << stateColor << stateStr << "\033[0m\n";
}

void Shell::cmdInfo(const std::vector<std::string>& args) {
    json info = client_->getServerInfo();
    if (!info.empty()) {
        std::cout << "服务器信息:\n" << info.dump(2) << "\n";
    } else {
        std::cout << "暂无服务器信息\n";
    }
}

void Shell::cmdClear(const std::vector<std::string>& args) {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

void Shell::printWelcome() {
    std::cout << "\n";
    std::cout << "=====================================\n";
    std::cout << "       Gametest Client Shell       \n";
    std::cout << "=====================================\n";
    std::cout << "✨ 支持的功能:\n";
    std::cout << "  • Tab 自动补全命令和参数\n";
    std::cout << "  • 命令历史记录 (上下箭头浏览)\n";
    std::cout << "  • 智能错误提示和建议\n";
    std::cout << "  • JSON 消息发送\n\n";
    std::cout << "输入 'help' 查看可用命令\n";
    std::cout << "输入 'quit' 退出程序\n\n";
}

} // namespace gametest