#pragma once

#include <string>
#include <vector>
#include <functional>
#include <map>
#include <memory>
#include <iomanip>

namespace gametest {

class GametestClient; // 前向声明

struct Command {
    std::string name;
    std::string description;
    std::function<void(const std::vector<std::string>&)> handler;
    std::string usage;
};

class Shell {
public:
    explicit Shell(std::shared_ptr<GametestClient> client);
    ~Shell();

    // 运行 shell
    void run();
    void stop();

    // 命令注册
    void registerCommand(const Command& command);

    // 提示符
    void setPrompt(const std::string& prompt) { prompt_ = prompt; }

    // 获取命令列表（用于自动补全）
    const std::map<std::string, Command>& getCommands() const { return commands_; }

private:
    // 命令处理
    void processCommand(const std::string& line);
    void parseCommand(const std::string& line, std::string& cmd, std::vector<std::string>& args);
    std::vector<std::string> splitArgs(const std::string& args);

    // 内建命令
    void cmdHelp(const std::vector<std::string>& args);
    void cmdQuit(const std::vector<std::string>& args);
    void cmdConnect(const std::vector<std::string>& args);
    void cmdDisconnect(const std::vector<std::string>& args);
    void cmdLogin(const std::vector<std::string>& args);
    void cmdAutoLogin(const std::vector<std::string>& args);
    void cmdSend(const std::vector<std::string>& args);
    void cmdStatus(const std::vector<std::string>& args);
    void cmdInfo(const std::vector<std::string>& args);
    void cmdClear(const std::vector<std::string>& args);

    // 实用功能
    void printWelcome();
    void printPrompt();
    int levenshtein_distance(const std::string& s1, const std::string& s2);

    // 成员变量
    std::shared_ptr<GametestClient> client_;
    std::map<std::string, Command> commands_;
    std::string prompt_;
    bool running_;
    std::string history_file_;
    std::vector<std::string> recent_hosts_;
};

} // namespace gametest