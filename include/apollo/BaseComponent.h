#pragma once

#include "IComponent.h"
#include <atomic>
#include <sstream>
#include <random>

namespace Apollo {

class BaseComponent : public IComponent {
public:
    BaseComponent(const std::string& name)
        : name_(name), state_(ComponentState::UNINITIALIZED) {
        guid_ = generateGuid();
    }

    ~BaseComponent() override = default;

    bool initialize() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ != ComponentState::UNINITIALIZED) {
            return false;
        }

        state_ = ComponentState::INITIALIZING;
        bool result = onInitialize();
        state_ = result ? ComponentState::INITIALIZED : ComponentState::UNINITIALIZED;
        return result;
    }

    bool start() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ != ComponentState::INITIALIZED) {
            return false;
        }

        state_ = ComponentState::STARTING;
        bool result = onStart();
        state_ = result ? ComponentState::STARTED : ComponentState::INITIALIZED;
        return result;
    }

    bool stop() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ != ComponentState::STARTED) {
            return false;
        }

        state_ = ComponentState::STOPPING;
        bool result = onStop();
        state_ = result ? ComponentState::STOPPED : ComponentState::STARTED;
        return result;
    }

    bool destroy() override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        if (state_ == ComponentState::DESTROYED || state_ == ComponentState::DESTROYING) {
            return false;
        }

        state_ = ComponentState::DESTROYING;

        if (state_ == ComponentState::STARTED) {
            stop();
        }

        bool result = onDestroy();
        state_ = ComponentState::DESTROYED;
        return result;
    }

    ComponentState getState() const override {
        return state_;
    }

    const std::string& getName() const override {
        return name_;
    }

    const std::string& getGuid() const override {
        return guid_;
    }

    void setState(ComponentState state) override {
        std::lock_guard<std::mutex> lock(stateMutex_);
        state_ = state;
    }

    bool dependsOn(const std::string& componentName) const override {
        for (const auto& dep : dependencies_) {
            if (dep == componentName) {
                return true;
            }
        }
        return false;
    }

    const std::vector<std::string>& getDependencies() const override {
        return dependencies_;
    }

protected:
    virtual bool onInitialize() { return true; }
    virtual bool onStart() { return true; }
    virtual bool onStop() { return true; }
    virtual bool onDestroy() { return true; }

    void addDependency(const std::string& componentName) {
        dependencies_.push_back(componentName);
    }

private:
    std::string generateGuid() {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, 15);

        std::stringstream ss;
        ss << std::hex;
        for (int i = 0; i < 32; ++i) {
            if (i == 8 || i == 12 || i == 16 || i == 20) {
                ss << "-";
            }
            ss << dis(gen);
        }
        return ss.str();
    }

    std::string name_;
    std::string guid_;
    std::atomic<ComponentState> state_;
    std::vector<std::string> dependencies_;
    mutable std::mutex stateMutex_;
};

}