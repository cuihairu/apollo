#pragma once

#include "BeanDefinition.h"
#include "ConfigEnvironment.h"
#include "IComponent.h"
#include "LifecycleProcessor.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <stdexcept>

namespace Apollo {

class ApplicationContext {
public:
    static ApplicationContext& getInstance() {
        static ApplicationContext instance;
        return instance;
    }

    bool registerComponent(const std::string& name, ComponentFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (definitions_.find(name) != definitions_.end()) {
            return false;
        }

        BeanDefinition definition;
        definition.name = name;
        definition.factory = std::move(factory);

        if (definition.factory) {
            auto probe = definition.factory();
            if (probe) {
                definition.phase = probe->getPhase();
                definition.dependencies = probe->getDependencies();
            }
        }

        definitions_[name] = std::move(definition);
        runtimeInfo_[name] = makeRuntimeInfo(definitions_[name]);
        return true;
    }

    template<typename T>
    bool registerComponent() {
        ComponentFactory factory = []() -> ComponentPtr {
            return std::make_shared<T>();
        };
        return registerComponent(T::getStaticName(), factory);
    }

    bool registerBeanDefinition(const BeanDefinition& definition) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (definition.name.empty() || !definition.factory) {
            return false;
        }
        if (definitions_.find(definition.name) != definitions_.end()) {
            return false;
        }

        definitions_[definition.name] = definition;
        runtimeInfo_[definition.name] = makeRuntimeInfo(definition);
        return true;
    }

    bool unregisterComponent(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        const auto factoryErased = definitions_.erase(name);
        components_.erase(name);
        runtimeInfo_.erase(name);
        return factoryErased > 0;
    }

    ComponentPtr getComponent(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = components_.find(name);
        if (it != components_.end()) {
            return it->second;
        }

        auto definitionIt = definitions_.find(name);
        if (definitionIt == definitions_.end()) {
            return nullptr;
        }

        ComponentPtr component = definitionIt->second.factory();
        components_[name] = component;
        auto runtimeIt = runtimeInfo_.find(name);
        if (runtimeIt != runtimeInfo_.end()) {
            runtimeIt->second.instantiated = (component != nullptr);
            runtimeIt->second.stage = component ? BeanLifecycleStage::Instantiated
                                                : BeanLifecycleStage::Failed;
            runtimeIt->second.componentState = component ? component->getState()
                                                         : ComponentState::UNINITIALIZED;
            runtimeIt->second.lastOperation = "instantiate";
            if (!component) {
                runtimeIt->second.lastError = "factory returned nullptr";
            }
        }
        return component;
    }

    template<typename T>
    std::shared_ptr<T> getComponent() {
        auto component = getComponent(T::getStaticName());
        return std::dynamic_pointer_cast<T>(component);
    }

    bool initializeComponents() {
        std::vector<std::string> initOrder = calculateDependencyOrder();
        std::vector<ComponentPtr> initialized;

        for (const auto& name : initOrder) {
            auto component = getComponent(name);
            if (!component || !component->initialize()) {
                updateFailureState(name, "initialize", "initialize() returned false");
                rollbackInitializedComponents(initialized);
                return false;
            }
            updateSuccessState(name, BeanLifecycleStage::Initialized, "initialize", component);
            initialized.push_back(component);
        }
        return true;
    }

    bool startComponents() {
        std::vector<std::string> startOrder = calculateDependencyOrder();
        std::vector<ComponentPtr> started;

        for (const auto& name : startOrder) {
            auto component = getComponent(name);
            if (!component || !component->start()) {
                updateFailureState(name, "start", "start() returned false");
                rollbackStartedComponents(started);
                return false;
            }
            updateSuccessState(name, BeanLifecycleStage::Started, "start", component);
            started.push_back(component);
        }
        return true;
    }

    bool stopComponents() {
        std::vector<std::string> stopOrder = calculateDependencyOrder();
        std::reverse(stopOrder.begin(), stopOrder.end());

        for (const auto& name : stopOrder) {
            auto component = getComponent(name);
            if (component) {
                component->stop();
                updateSuccessState(name, BeanLifecycleStage::Stopped, "stop", component);
            }
        }
        return true;
    }

    bool destroyComponents() {
        std::vector<std::string> destroyOrder = calculateDependencyOrder();
        std::reverse(destroyOrder.begin(), destroyOrder.end());

        for (const auto& name : destroyOrder) {
            auto component = getComponent(name);
            if (component) {
                component->destroy();
                updateSuccessState(name, BeanLifecycleStage::Destroyed, "destroy", component);
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        components_.clear();
        return true;
    }

    std::vector<std::string> getComponentNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& pair : definitions_) {
            names.push_back(pair.first);
        }
        std::sort(names.begin(), names.end());
        return names;
    }

    std::vector<std::string> getDependencyOrder() {
        return calculateDependencyOrder();
    }

    std::vector<BeanDefinition> getBeanDefinitions() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BeanDefinition> result;
        result.reserve(definitions_.size());
        for (const auto& [_, definition] : definitions_) {
            result.push_back(definition);
        }
        std::sort(result.begin(), result.end(),
            [](const BeanDefinition& lhs, const BeanDefinition& rhs) {
                return lhs.name < rhs.name;
            });
        return result;
    }

    bool hasBeanDefinition(const std::string& name) const {
        std::lock_guard<std::mutex> lock(mutex_);
        return definitions_.find(name) != definitions_.end();
    }

    std::vector<BeanRuntimeInfo> getBeanRuntimeInfo() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<BeanRuntimeInfo> result;
        result.reserve(runtimeInfo_.size());
        for (const auto& [_, info] : runtimeInfo_) {
            result.push_back(info);
        }
        std::sort(result.begin(), result.end(),
            [](const BeanRuntimeInfo& lhs, const BeanRuntimeInfo& rhs) {
                return lhs.name < rhs.name;
            });
        return result;
    }

    bool getBeanRuntimeInfo(const std::string& name, BeanRuntimeInfo& out) const {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = runtimeInfo_.find(name);
        if (it == runtimeInfo_.end()) {
            return false;
        }
        out = it->second;
        return true;
    }

    std::shared_ptr<ConfigEnvironment> getConfigEnvironment() {
        std::lock_guard<std::mutex> lock(mutex_);
        return configEnvironment_;
    }

    void setConfigEnvironment(std::shared_ptr<ConfigEnvironment> configEnvironment) {
        std::lock_guard<std::mutex> lock(mutex_);
        configEnvironment_ = configEnvironment ? std::move(configEnvironment)
                                               : std::make_shared<ConfigEnvironment>();
    }

    void syncConfigEnvironmentToManager() {
        auto configEnvironment = getConfigEnvironment();
        if (configEnvironment) {
            configEnvironment->syncToConfigManager(*ConfigManager::getInstance());
        }
    }

private:
    ApplicationContext()
        : configEnvironment_(std::make_shared<ConfigEnvironment>()) {}
    ~ApplicationContext() = default;
    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

    std::vector<std::string> calculateDependencyOrder() {
        std::lock_guard<std::mutex> lock(mutex_);
        return LifecycleProcessor::sortBeanDefinitions(definitions_);
    }

    static BeanRuntimeInfo makeRuntimeInfo(const BeanDefinition& definition) {
        BeanRuntimeInfo info;
        info.name = definition.name;
        info.autoStart = definition.autoStart;
        info.phase = definition.phase;
        info.dependencies = definition.dependencies;
        info.lastOperation = "register";
        return info;
    }

    void updateSuccessState(const std::string& name,
                            BeanLifecycleStage stage,
                            const std::string& operation,
                            const ComponentPtr& component) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = runtimeInfo_.find(name);
        if (it == runtimeInfo_.end()) {
            return;
        }
        it->second.stage = stage;
        it->second.instantiated = (component != nullptr);
        it->second.componentState = component ? component->getState()
                                              : ComponentState::UNINITIALIZED;
        it->second.lastOperation = operation;
        it->second.lastError.clear();
    }

    void updateFailureState(const std::string& name,
                            const std::string& operation,
                            const std::string& error) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = runtimeInfo_.find(name);
        if (it == runtimeInfo_.end()) {
            return;
        }
        it->second.stage = BeanLifecycleStage::Failed;
        it->second.lastOperation = operation;
        it->second.lastError = error;
        auto componentIt = components_.find(name);
        if (componentIt != components_.end() && componentIt->second) {
            it->second.instantiated = true;
            it->second.componentState = componentIt->second->getState();
        }
    }

    void rollbackInitializedComponents(const std::vector<ComponentPtr>& initialized) {
        for (auto it = initialized.rbegin(); it != initialized.rend(); ++it) {
            if (*it) {
                const auto& name = (*it)->getName();
                (*it)->destroy();
                updateSuccessState(name, BeanLifecycleStage::Destroyed, "rollback-destroy", *it);
            }
        }
    }

    void rollbackStartedComponents(const std::vector<ComponentPtr>& started) {
        for (auto it = started.rbegin(); it != started.rend(); ++it) {
            if (*it) {
                const auto& name = (*it)->getName();
                (*it)->stop();
                updateSuccessState(name, BeanLifecycleStage::Stopped, "rollback-stop", *it);
                (*it)->destroy();
                updateSuccessState(name, BeanLifecycleStage::Destroyed, "rollback-destroy", *it);
            }
        }
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, BeanDefinition> definitions_;
    std::unordered_map<std::string, ComponentPtr> components_;
    std::unordered_map<std::string, BeanRuntimeInfo> runtimeInfo_;
    std::shared_ptr<ConfigEnvironment> configEnvironment_;
};

}
