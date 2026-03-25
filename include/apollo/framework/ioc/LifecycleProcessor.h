#pragma once

#include "BeanDefinition.h"
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace Apollo {

class LifecycleProcessor {
public:
    static std::vector<std::string> sortBeanDefinitions(
        const std::unordered_map<std::string, BeanDefinition>& definitions) {
        std::unordered_map<std::string, int> inDegree;
        std::unordered_map<std::string, std::vector<std::string>> adjList;
        std::unordered_map<std::string, int> phases;
        std::vector<std::string> result;

        auto compareByPhaseAndName = [&](const std::string& lhs, const std::string& rhs) {
            const int lhsPhase = phases.at(lhs);
            const int rhsPhase = phases.at(rhs);
            if (lhsPhase != rhsPhase) {
                return lhsPhase > rhsPhase;
            }
            return lhs > rhs;
        };

        std::priority_queue<std::string, std::vector<std::string>, decltype(compareByPhaseAndName)>
            ready(compareByPhaseAndName);

        for (const auto& [name, definition] : definitions) {
            inDegree[name] = 0;
            adjList[name] = {};
            phases[name] = definition.phase;
        }

        for (const auto& [name, definition] : definitions) {
            for (const auto& dep : definition.dependencies) {
                if (definitions.find(dep) == definitions.end()) {
                    throw std::runtime_error(
                        "Missing dependency '" + dep + "' required by component '" + name + "'");
                }
                adjList[dep].push_back(name);
                inDegree[name]++;
            }
        }

        for (const auto& [name, degree] : inDegree) {
            if (degree == 0) {
                ready.push(name);
            }
        }

        while (!ready.empty()) {
            std::string current = ready.top();
            ready.pop();
            result.push_back(current);

            auto& neighbors = adjList[current];
            std::sort(neighbors.begin(), neighbors.end(),
                [&](const std::string& lhs, const std::string& rhs) {
                    const int lhsPhase = phases.at(lhs);
                    const int rhsPhase = phases.at(rhs);
                    if (lhsPhase != rhsPhase) {
                        return lhsPhase < rhsPhase;
                    }
                    return lhs < rhs;
                });

            for (const auto& neighbor : neighbors) {
                inDegree[neighbor]--;
                if (inDegree[neighbor] == 0) {
                    ready.push(neighbor);
                }
            }
        }

        if (result.size() != definitions.size()) {
            throw std::runtime_error("Circular dependency detected");
        }

        return result;
    }
};

} // namespace Apollo
