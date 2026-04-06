#pragma once

#include "apollo/runtime/application_host.hpp"

#include <deque>
#include <mutex>
#include <string>

namespace apollo::runtime {

class QueueConsoleEventSource final : public IConsoleEventSource {
public:
    QueueConsoleEventSource() = default;

    void push_command(std::string command);
    bool poll(ConsoleEvent& event) override;

private:
    std::mutex mutex_;
    std::deque<std::string> commands_;
};

using ConsoleEventSource = QueueConsoleEventSource;

} // namespace apollo::runtime
