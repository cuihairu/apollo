#include "apollo/FileWatcher.h"
#include <algorithm>

namespace Apollo {

FileWatcher::FileWatcher()
    : running_(false)
    , pollInterval_(std::chrono::milliseconds(1000))
#ifdef _WIN32
    , directoryHandle_(INVALID_HANDLE_VALUE)
#else
#ifndef USE_POLLING_WATCHER
    , inotifyInstance_(-1)
#endif
#endif
{
}

FileWatcher::~FileWatcher() {
    stop();
}

bool FileWatcher::addWatch(const std::string& path, FileChangeCallback callback) {
    std::lock_guard<std::mutex> lock(watchesMutex_);

    WatchInfo info;
    info.path = path;
    info.callback = callback;
    info.lastModified = getFileModificationTime(path);
    info.exists = info.lastModified > 0;

    watches_[path] = info;

#ifdef _WIN32
    if (directoryHandle_ == INVALID_HANDLE_VALUE) {
        std::string directory = path;
        size_t lastSlash = directory.find_last_of("\\/");
        if (lastSlash != std::string::npos) {
            directory = directory.substr(0, lastSlash);
        }

        directoryHandle_ = CreateFileA(
            directory.c_str(),
            FILE_LIST_DIRECTORY,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            NULL,
            OPEN_EXISTING,
            FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
            NULL
        );
    }
#else
#ifndef USE_POLLING_WATCHER
    if (inotifyInstance_ == -1) {
        inotifyInstance_ = inotify_init1(IN_NONBLOCK);
    }

    if (inotifyInstance_ != -1) {
        std::string directory = path;
        size_t lastSlash = directory.find_last_of("/");
        if (lastSlash != std::string::npos) {
            directory = directory.substr(0, lastSlash);
        }

        int wd = inotify_add_watch(inotifyInstance_, directory.c_str(),
                                   IN_MODIFY | IN_MOVED_TO | IN_CREATE | IN_DELETE);
        if (wd != -1) {
            watchDescriptors_[wd] = directory;
        }
    }
#endif
#endif

    return true;
}

bool FileWatcher::removeWatch(const std::string& path) {
    std::lock_guard<std::mutex> lock(watchesMutex_);
    return watches_.erase(path) > 0;
}

bool FileWatcher::start() {
    if (running_) {
        return false;
    }

    running_ = true;
    watchThread_ = std::thread(&FileWatcher::watchLoop, this);
    return true;
}

bool FileWatcher::stop() {
    if (!running_) {
        return false;
    }

    running_ = false;

    if (watchThread_.joinable()) {
        watchThread_.join();
    }

#ifdef _WIN32
    if (directoryHandle_ != INVALID_HANDLE_VALUE) {
        CloseHandle(directoryHandle_);
        directoryHandle_ = INVALID_HANDLE_VALUE;
    }
#else
#ifndef USE_POLLING_WATCHER
    if (inotifyInstance_ != -1) {
        for (const auto& pair : watchDescriptors_) {
            inotify_rm_watch(inotifyInstance_, pair.first);
        }
        watchDescriptors_.clear();
        close(inotifyInstance_);
        inotifyInstance_ = -1;
    }
#endif
#endif

    return true;
}

bool FileWatcher::isRunning() const {
    return running_;
}

void FileWatcher::setPollInterval(std::chrono::milliseconds interval) {
    pollInterval_ = interval;
}

std::chrono::milliseconds FileWatcher::getPollInterval() const {
    return pollInterval_;
}

void FileWatcher::watchLoop() {
    while (running_) {
        checkForChanges();
        std::this_thread::sleep_for(pollInterval_);
    }
}

void FileWatcher::checkForChanges() {
    std::lock_guard<std::mutex> lock(watchesMutex_);

    for (auto& pair : watches_) {
        WatchInfo& info = pair.second;
        long long currentModified = getFileModificationTime(info.path);
        bool currentlyExists = currentModified > 0;

        if (info.exists && currentlyExists) {
            if (currentModified > info.lastModified) {
                info.lastModified = currentModified;
                if (info.callback) {
                    info.callback(info.path);
                }
            }
        } else if (info.exists != currentlyExists) {
            info.exists = currentlyExists;
            info.lastModified = currentModified;
            if (info.callback) {
                info.callback(info.path);
            }
        }
    }
}

long long FileWatcher::getFileModificationTime(const std::string& path) {
#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (GetFileAttributesExA(path.c_str(), GetFileExInfoStandard, &data)) {
        LARGE_INTEGER time;
        time.LowPart = data.ftLastWriteTime.dwLowDateTime;
        time.HighPart = data.ftLastWriteTime.dwHighDateTime;
        return time.QuadPart;
    }
#else
    struct stat fileStat;
    if (stat(path.c_str(), &fileStat) == 0) {
        return static_cast<long long>(fileStat.st_mtime);
    }
#endif
    return -1;
}

}
