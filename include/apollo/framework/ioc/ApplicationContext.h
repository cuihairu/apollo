#pragma once

#include "IComponent.h"
#include <unordered_map>
#include <vector>
#include <memory>
#include <mutex>
#include <algorithm>
#include <queue>

namespace Apollo {

class ApplicationContext {
public:
    static ApplicationContext& getInstance() {
        static ApplicationContext instance;
        return instance;
    }

    bool registerComponent(const std::string& name, ComponentFactory factory) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (factories_.find(name) != factories_.end()) {
            return false;
        }

        factories_[name] = factory;
        return true;
    }

    template<typename T>
    bool registerComponent() {
        auto factory = []() -> ComponentPtr {
            return std::make_shared<T>();
        };
        return registerComponent(T::getStaticName(), factory);
    }

    ComponentPtr getComponent(const std::string& name) {
        std::lock_guard<std::mutex> lock(mutex_);

        auto it = components_.find(name);
        if (it != components_.end()) {
            return it->second;
        }

        auto factoryIt = factories_.find(name);
        if (factoryIt == factories_.end()) {
            return nullptr;
        }

        ComponentPtr component = factoryIt->second();
        components_[name] = component;
        return component;
    }

    template<typename T>
    std::shared_ptr<T> getComponent() {
        auto component = getComponent(T::getStaticName());
        return std::dynamic_pointer_cast<T>(component);
    }

    bool initializeComponents() {
        std::vector<std::string> initOrder = calculateDependencyOrder();

        for (const auto& name : initOrder) {
            auto component = getComponent(name);
            if (!component || !component->initialize()) {
                return false;
            }
        }
        return true;
    }

    bool startComponents() {
        std::vector<std::string> startOrder = calculateDependencyOrder();

        for (const auto& name : startOrder) {
            auto component = getComponent(name);
            if (!component || !component->start()) {
                return false;
            }
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
            }
        }

        std::lock_guard<std::mutex> lock(mutex_);
        components_.clear();
        return true;
    }

    std::vector<std::string> getComponentNames() const {
        std::lock_guard<std::mutex> lock(mutex_);
        std::vector<std::string> names;
        for (const auto& pair : factories_) {
            names.push_back(pair.first);
        }
        return names;
    }

private:
    ApplicationContext() = default;
    ~ApplicationContext() = default;
    ApplicationContext(const ApplicationContext&) = delete;
    ApplicationContext& operator=(const ApplicationContext&) = delete;

    std::vector<std::string> calculateDependencyOrder() {
        std::unordered_map<std::string, int> inDegree;
        std::unordered_map<std::string, std::vector<std::string>> adjList;
        std::queue<std::string> queue;
        std::vector<std::string> result;

        for (const auto& pair : factories_) {
            const std::string& name = pair.first;
            inDegree[name] = 0;
            adjList[name] = {};
        }

        for (const auto& pair : factories_) {
            const std::string& name = pair.first;
            auto tempComponent = pair.second();

            for (const auto& dep : tempComponent->getDependencies()) {
                if (factories_.find(dep) != factories_.end()) {
                    adjList[dep].push_back(name);
                    inDegree[name]++;
                }
            }
        }

        for (const auto& pair : inDegree) {
            if (pair.second == 0) {
                queue.push(pair.first);
            }
        }

        while (!queue.empty()) {
            std::string current = queue.front();
            queue.pop();
            result.push_back(current);

            for (const auto& neighbor : adjList[current]) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    queue.push(neighbor);
                }
            }
        }

        if (result.size() != factories_.size()) {
            throw std::runtime_error("Circular dependency detected");
        }

        return result;
    }

    mutable std::mutex mutex_;
    std::unordered_map<std::string, ComponentFactory> factories_;
    std::unordered_map<std::string, ComponentPtr> components_;
};

}