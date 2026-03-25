#pragma once

#include "IComponent.h"
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

namespace Apollo {

struct BeanDefinition {
    std::string name;
    ComponentFactory factory;
    int phase = static_cast<int>(LifecyclePhase::CoreService);
    bool autoStart = true;
    bool lazyInit = false;
    std::vector<std::string> dependencies;
    std::unordered_map<std::string, std::string> metadata;
};

enum class BeanLifecycleStage {
    Registered,
    Instantiated,
    Initialized,
    Started,
    Stopped,
    Destroyed,
    Failed
};

struct BeanRuntimeInfo {
    std::string name;
    BeanLifecycleStage stage = BeanLifecycleStage::Registered;
    ComponentState componentState = ComponentState::UNINITIALIZED;
    bool instantiated = false;
    bool autoStart = true;
    int phase = static_cast<int>(LifecyclePhase::CoreService);
    std::vector<std::string> dependencies;
    std::string lastOperation;
    std::string lastError;
};

} // namespace Apollo
