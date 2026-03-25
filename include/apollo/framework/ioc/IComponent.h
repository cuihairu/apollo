#pragma once

#include <string>
#include <memory>
#include <unordered_map>
#include <functional>
#include <typeinfo>
#include <vector>
#include <mutex>

namespace Apollo {

enum class LifecyclePhase : int {
    Bootstrap = -2000,
    Infrastructure = -1000,
    Config = -500,
    Data = 0,
    CoreService = 500,
    Business = 1000,
    Gateway = 2000
};

enum class ComponentState {
    UNINITIALIZED,
    INITIALIZING,
    INITIALIZED,
    STARTING,
    STARTED,
    STOPPING,
    STOPPED,
    DESTROYING,
    DESTROYED
};

class IComponent {
public:
    virtual ~IComponent() = default;

    virtual bool initialize() = 0;
    virtual bool start() = 0;
    virtual bool stop() = 0;
    virtual bool destroy() = 0;

    virtual ComponentState getState() const = 0;
    virtual const std::string& getName() const = 0;
    virtual const std::string& getGuid() const = 0;

    virtual void setState(ComponentState state) = 0;
    virtual bool dependsOn(const std::string& componentName) const = 0;
    virtual const std::vector<std::string>& getDependencies() const = 0;
    virtual int getPhase() const {
        return static_cast<int>(LifecyclePhase::CoreService);
    }
};

using ComponentPtr = std::shared_ptr<IComponent>;
using ComponentFactory = std::function<ComponentPtr()>;

}
