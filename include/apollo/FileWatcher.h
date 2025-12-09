#pragma once

#include <string>
#include <functional>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#include <poll.h>

// For macOS, we don't have inotify
#ifdef __APPLE__
#define USE_POLLING_WATCHER 1
#else
#include <sys/inotify.h>
#endif
#endif

namespace Apollo {

class FileWatcher {
public:
    using FileChangeCallback = std::function<void(const std::string&)>;

    FileWatcher();
    ~FileWatcher();

    bool addWatch(const std::string& path, FileChangeCallback callback);
    bool removeWatch(const std::string& path);
    bool start();
    bool stop();
    bool isRunning() const;

    void setPollInterval(std::chrono::milliseconds interval);
    std::chrono::milliseconds getPollInterval() const;

private:
    void watchLoop();
    void checkForChanges();
    long long getFileModificationTime(const std::string& path);

    struct WatchInfo {
        std::string path;
        FileChangeCallback callback;
        long long lastModified;
        bool exists;
    };

    std::unordered_map<std::string, WatchInfo> watches_;
    std::thread watchThread_;
    std::atomic<bool> running_;
    mutable std::mutex watchesMutex_;
    std::chrono::milliseconds pollInterval_;

#ifdef _WIN32
    HANDLE directoryHandle_;
#else
    int inotifyInstance_;
    std::unordered_map<int, std::string> watchDescriptors_;
#endif
};

}