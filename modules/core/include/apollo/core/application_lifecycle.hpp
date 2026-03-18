#pragma once

#include <string_view>

namespace apollo::core {

enum class ApplicationPhase {
    Boot,
    ConfigLoaded,
    Initialized,
    Ready,
    Stopping,
    Stopped,
};

struct ApplicationReloadContext {
    std::string_view reason;
};

class IApplicationLifecycle {
public:
    virtual ~IApplicationLifecycle() = default;

    virtual void on_application_boot() {}
    virtual void on_application_config_loaded() {}
    virtual void on_application_initialized() {}
    virtual void on_application_ready() {}
    virtual void on_application_reload(const ApplicationReloadContext&) {}
    virtual void on_application_stop() {}
};

} // namespace apollo::core
