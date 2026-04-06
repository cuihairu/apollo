#include "apollo/runtime/signal_source.hpp"

#include <array>
#include <csignal>
#include <mutex>

namespace apollo::runtime {
namespace {

constexpr std::size_t kMaxTrackedSignals = 64;

std::mutex g_signal_mutex;
std::array<std::sig_atomic_t, kMaxTrackedSignals> g_pending_signals{};
std::array<std::size_t, kMaxTrackedSignals> g_handler_ref_counts{};
std::array<void (*)(int), kMaxTrackedSignals> g_previous_handlers{};

bool is_trackable_signal(int signal_value) {
    return signal_value >= 0 &&
           static_cast<std::size_t>(signal_value) < kMaxTrackedSignals;
}

void process_signal_handler(int signal_value) {
    if (is_trackable_signal(signal_value)) {
        g_pending_signals[static_cast<std::size_t>(signal_value)] = 1;
    }
}

} // namespace

ProcessSignalSource::ProcessSignalSource()
    : ProcessSignalSource({SIGINT, SIGTERM}) {}

ProcessSignalSource::ProcessSignalSource(std::initializer_list<int> signals)
    : signals_(signals) {
    install_handlers();
}

ProcessSignalSource::~ProcessSignalSource() {
    uninstall_handlers();
}

bool ProcessSignalSource::poll(SignalEvent& event) {
    std::lock_guard<std::mutex> lock(g_signal_mutex);
    for (const int signal_value : signals_) {
        if (!is_trackable_signal(signal_value)) {
            continue;
        }

        const auto index = static_cast<std::size_t>(signal_value);
        if (g_pending_signals[index] == 0) {
            continue;
        }

        g_pending_signals[index] = 0;
        event.value = signal_value;
        return true;
    }

    return false;
}

void ProcessSignalSource::install_handlers() {
    std::lock_guard<std::mutex> lock(g_signal_mutex);
    for (const int signal_value : signals_) {
        if (!is_trackable_signal(signal_value)) {
            continue;
        }

        const auto index = static_cast<std::size_t>(signal_value);
        if (g_handler_ref_counts[index] == 0) {
            g_previous_handlers[index] = std::signal(signal_value, process_signal_handler);
        }
        ++g_handler_ref_counts[index];
    }
}

void ProcessSignalSource::uninstall_handlers() {
    std::lock_guard<std::mutex> lock(g_signal_mutex);
    for (const int signal_value : signals_) {
        if (!is_trackable_signal(signal_value)) {
            continue;
        }

        const auto index = static_cast<std::size_t>(signal_value);
        if (g_handler_ref_counts[index] == 0) {
            continue;
        }

        --g_handler_ref_counts[index];
        if (g_handler_ref_counts[index] == 0) {
            std::signal(signal_value, g_previous_handlers[index]);
            g_pending_signals[index] = 0;
            g_previous_handlers[index] = SIG_DFL;
        }
    }
}

} // namespace apollo::runtime
