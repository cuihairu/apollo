#include "apollo/framework/ioc/ConfigManager.h"
#include "apollo/utils/io/FileWatcher.h"
#include <fstream>
#include <sstream>

#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
using json = nlohmann::json;
#endif

namespace Apollo {

bool ConfigManager::loadFromFile(const std::string& filename) {
#ifdef HAVE_NLOHMANN_JSON
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    file.close();

    return loadFromJson(content);
#else
    // Simple key=value format without JSON
    std::ifstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    values_.clear();
    typedValues_.clear();

    std::string line;
    while (std::getline(file, line)) {
        size_t pos = line.find('=');
        if (pos != std::string::npos) {
            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);
            values_[key] = value;
        }
    }

    file.close();
    return true;
#endif
}

bool ConfigManager::saveToFile(const std::string& filename) {
#ifdef HAVE_NLOHMANN_JSON
    std::string jsonContent = saveToJson();
    if (jsonContent.empty()) {
        return false;
    }

    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    file << jsonContent;
    file.close();
    return true;
#else
    // Simple key=value format without JSON
    std::ofstream file(filename);
    if (!file.is_open()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (const auto& [key, value] : values_) {
        file << key << "=" << value << std::endl;
    }

    file.close();
    return true;
#endif
}

bool ConfigManager::loadFromJson(const std::string& jsonStr) {
#ifdef HAVE_NLOHMANN_JSON
    try {
        json j = json::parse(jsonStr);
        std::lock_guard<std::mutex> lock(mutex_);
        values_.clear();
        typedValues_.clear();

        for (auto& [key, value] : j.items()) {
            if (value.is_string()) {
                values_[key] = value.get<std::string>();
            } else if (value.is_number_integer()) {
                values_[key] = std::to_string(value.get<int64_t>());
            } else if (value.is_number_float()) {
                values_[key] = std::to_string(value.get<double>());
            } else if (value.is_boolean()) {
                values_[key] = value.get<bool>() ? "true" : "false";
            } else {
                values_[key] = value.dump();
            }
        }

        return true;
    } catch (const json::parse_error&) {
        return false;
    }
#else
    // Simple implementation without JSON support
    return false;
#endif
}

std::string ConfigManager::saveToJson() const {
#ifdef HAVE_NLOHMANN_JSON
    std::lock_guard<std::mutex> lock(mutex_);
    json j;

    for (const auto& [key, value] : values_) {
        try {
            if (value == "true" || value == "false") {
                j[key] = (value == "true");
            } else {
                try {
                    int64_t intValue = std::stoll(value);
                    j[key] = intValue;
                } catch (...) {
                    try {
                        double doubleValue = std::stod(value);
                        j[key] = doubleValue;
                    } catch (...) {
                        j[key] = value;
                    }
                }
            }
        } catch (...) {
            j[key] = value;
        }
    }

    return j.dump(2);
#else
    // Simple key=value format without JSON
    std::lock_guard<std::mutex> lock(mutex_);
    std::string result;
    for (const auto& [key, value] : values_) {
        result += key + "=" + value + "\n";
    }
    return result;
#endif
}

void ConfigManager::addChangeListener(const std::string& key, std::function<void(const std::string&)> listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeListeners_[key].push_back(listener);
}

void ConfigManager::removeChangeListener(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);
    changeListeners_.erase(key);
}

void ConfigManager::addGlobalChangeListener(std::function<void(const std::string&)> listener) {
    std::lock_guard<std::mutex> lock(mutex_);
    globalChangeListeners_.push_back(listener);
}

void ConfigManager::removeGlobalChangeListener(std::function<void(const std::string&)> listener) {
    (void)listener;
    // Note: std::function cannot be compared directly, so we use a different approach
    // For now, this method is not implemented - consider using listener IDs instead
    // std::lock_guard<std::mutex> lock(mutex_);
    // globalChangeListeners_.erase(
    //     std::remove_if(globalChangeListeners_.begin(), globalChangeListeners_.end(),
    //                    [&listener](const std::function<void(const std::string&)>& existing) {
    //                        // Cannot directly compare std::function objects
    //                        return false;
    //                    }),
    //     globalChangeListeners_.end());
}

bool ConfigManager::enableAutoReload(const std::string& configFile) {
    disableAutoReload();

    fileWatcher_ = new FileWatcher();
    watchedFile_ = configFile;

    fileWatcher_->addWatch(configFile, [this](const std::string& path) {
        this->onFileChanged(path);
    });

    return fileWatcher_->start();
}

void ConfigManager::disableAutoReload() {
    if (fileWatcher_) {
        fileWatcher_->stop();
        delete fileWatcher_;
        fileWatcher_ = nullptr;
        watchedFile_.clear();
    }
}

bool ConfigManager::isAutoReloadEnabled() const {
    return fileWatcher_ != nullptr;
}

void ConfigManager::notifyChangeListeners(const std::string& key) {
    auto keyListeners = changeListeners_.find(key);
    if (keyListeners != changeListeners_.end()) {
        for (const auto& listener : keyListeners->second) {
            listener(key);
        }
    }

    for (const auto& listener : globalChangeListeners_) {
        listener(key);
    }
}

void ConfigManager::onFileChanged(const std::string& filename) {
    loadFromFile(filename);
}

}
