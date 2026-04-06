#include "apollo/runtime/console_event_source.hpp"

namespace apollo::runtime {

void QueueConsoleEventSource::push_command(std::string command) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back(std::move(command));
}

bool QueueConsoleEventSource::poll(ConsoleEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (commands_.empty()) {
        return false;
    }

    event.command = std::move(commands_.front());
    commands_.pop_front();
    return true;
}

} // namespace apollo::runtime
