#pragma once

#include "ApplicationContext.h"
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <stack>
#include <stdexcept>

namespace Apollo {

class DependencyManager {
public:
    static DependencyManager& getInstance() {
        static DependencyManager instance;
        return instance;
    }

    void addDependency(const std::string& component, const std::string& dependency) {
        dependencies_[component].insert(dependency);
    }

    void removeDependency(const std::string& component, const std::string& dependency) {
        auto it = dependencies_.find(component);
        if (it != dependencies_.end()) {
            it->second.erase(dependency);
        }
    }

    std::vector<std::string> getInitializationOrder() {
        return topologicalSort();
    }

    std::vector<std::string> getShutdownOrder() {
        auto initOrder = getInitializationOrder();
        std::reverse(initOrder.begin(), initOrder.end());
        return initOrder;
    }

    bool hasCircularDependency() {
        try {
            topologicalSort();
            return false;
        } catch (const std::runtime_error&) {
            return true;
        }
    }

    std::vector<std::string> findCircularDependency() {
        std::unordered_map<std::string, std::string> parent;
        std::unordered_set<std::string> visited;
        std::unordered_set<std::string> recStack;

        for (const auto& pair : dependencies_) {
            const std::string& node = pair.first;
            if (visited.find(node) == visited.end()) {
                std::vector<std::string> cycle;
                if (hasCycleDFS(node, visited, recStack, parent, cycle)) {
                    return cycle;
                }
            }
        }

        return {};
    }

    std::vector<std::string> getMissingDependencies() {
        std::vector<std::string> missing;
        auto& context = ApplicationContext::getInstance();
        auto registeredComponents = context.getComponentNames();

        for (const auto& pair : dependencies_) {
            for (const std::string& dep : pair.second) {
                if (std::find(registeredComponents.begin(), registeredComponents.end(), dep)
                    == registeredComponents.end()) {
                    missing.push_back(dep);
                }
            }
        }

        return missing;
    }

    bool validateDependencies() {
        auto missing = getMissingDependencies();
        if (!missing.empty()) {
            return false;
        }

        return !hasCircularDependency();
    }

    std::unordered_set<std::string> getTransitiveDependencies(const std::string& component) {
        std::unordered_set<std::string> result;
        std::unordered_set<std::string> visited;
        std::stack<std::string> stack;

        stack.push(component);
        while (!stack.empty()) {
            std::string current = stack.top();
            stack.pop();

            if (visited.find(current) != visited.end()) {
                continue;
            }
            visited.insert(current);

            auto it = dependencies_.find(current);
            if (it != dependencies_.end()) {
                for (const std::string& dep : it->second) {
                    if (result.find(dep) == result.end()) {
                        result.insert(dep);
                        stack.push(dep);
                    }
                }
            }
        }

        result.erase(component);
        return result;
    }

private:
    DependencyManager() = default;
    ~DependencyManager() = default;
    DependencyManager(const DependencyManager&) = delete;
    DependencyManager& operator=(const DependencyManager&) = delete;

    std::vector<std::string> topologicalSort() {
        std::unordered_map<std::string, int> inDegree;
        std::unordered_map<std::string, std::vector<std::string>> adjList;
        std::vector<std::string> allNodes;

        for (const auto& pair : dependencies_) {
            const std::string& node = pair.first;
            allNodes.push_back(node);
            inDegree[node] = 0;
            adjList[node] = {};

            for (const std::string& dep : pair.second) {
                allNodes.push_back(dep);
                inDegree[dep] = 0;
                adjList[dep] = {};
            }
        }

        for (const auto& pair : dependencies_) {
            const std::string& node = pair.first;
            for (const std::string& dep : pair.second) {
                adjList[dep].push_back(node);
                inDegree[node]++;
            }
        }

        std::vector<std::string> result;
        for (const auto& pair : inDegree) {
            if (pair.second == 0) {
                result.push_back(pair.first);
            }
        }

        for (size_t i = 0; i < result.size(); ++i) {
            std::string current = result[i];
            for (const std::string& neighbor : adjList[current]) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    result.push_back(neighbor);
                }
            }
        }

        if (result.size() != inDegree.size()) {
            auto cycle = findCircularDependency();
            std::string cycleStr;
            for (size_t i = 0; i < cycle.size(); ++i) {
                if (i > 0) cycleStr += " -> ";
                cycleStr += cycle[i];
            }
            throw std::runtime_error("Circular dependency detected: " + cycleStr);
        }

        return result;
    }

    bool hasCycleDFS(const std::string& node,
                    std::unordered_set<std::string>& visited,
                    std::unordered_set<std::string>& recStack,
                    std::unordered_map<std::string, std::string>& parent,
                    std::vector<std::string>& cycle) {
        visited.insert(node);
        recStack.insert(node);

        auto it = dependencies_.find(node);
        if (it != dependencies_.end()) {
            for (const std::string& neighbor : it->second) {
                if (recStack.find(neighbor) != recStack.end()) {
                    cycle.clear();
                    std::string current = node;
                    cycle.push_back(neighbor);
                    while (current != neighbor) {
                        cycle.push_back(current);
                        current = parent[current];
                    }
                    cycle.push_back(neighbor);
                    std::reverse(cycle.begin(), cycle.end());
                    return true;
                }

                if (visited.find(neighbor) == visited.end()) {
                    parent[neighbor] = node;
                    if (hasCycleDFS(neighbor, visited, recStack, parent, cycle)) {
                        return true;
                    }
                }
            }
        }

        recStack.erase(node);
        return false;
    }

    std::unordered_map<std::string, std::unordered_set<std::string>> dependencies_;
};

}