#pragma once

#include "apollo/actor/message.h"
#include <string>
#include <memory>
#include <functional>
#include <future>
#include <unordered_map>

namespace apollo {
namespace actor {

//==============================================================================
// 前向声明
//==============================================================================

class ActorCell;
class ActorSystem;

//==============================================================================
// Actor 标识（64位编码，复用 ServiceId）
//==============================================================================

struct ActorPath {
    std::string system;      // 系统名称
    std::string address;     // 地址（数据中心的机器标识）
    std::string name;        // Actor 名称
    uint32_t instance;       // 实例编号

    ActorPath() : instance(0) {}

    ActorPath(const std::string& s, const std::string& a,
              const std::string& n, uint32_t i = 0)
        : system(s), address(a), name(n), instance(i) {}

    // 完整路径字符串
    std::string toString() const {
        if (address.empty() || address == "0") {
            return system + "://" + name;
        }
        return system + "://" + address + "/" + name;
    }

    // 从字符串解析
    static ActorPath fromString(const std::string& path) {
        ActorPath result;
        size_t schemePos = path.find("://");
        if (schemePos != std::string::npos) {
            result.system = path.substr(0, schemePos);

            size_t slashPos = path.find('/', schemePos + 3);
            if (slashPos != std::string::npos) {
                result.address = path.substr(schemePos + 3, slashPos - schemePos - 3);
                result.name = path.substr(slashPos + 1);
            } else {
                result.name = path.substr(schemePos + 3);
            }
        } else {
            result.name = path;
        }
        return result;
    }

    // 是否为本地路径
    bool isLocal() const {
        return address.empty() || address == "0" || address == "localhost";
    }

    // 哈希值（用于路由）
    size_t hash() const {
        return std::hash<std::string>{}(toString());
    }

    bool operator==(const ActorPath& other) const {
        return toString() == other.toString();
    }
};

//==============================================================================
// Actor 引用（位置透明的句柄）
//==============================================================================

class ActorRef {
public:
    ActorRef() = default;

    ActorRef(const ActorPath& path, std::shared_ptr<ActorCell> cell = nullptr);

    // 获取路径
    const ActorPath& path() const { return path_; }

    // 设置路径
    void setPath(const ActorPath& path) { path_ = path; }

    // 是否有效
    bool isValid() const { return !path_.name.empty(); }

    // 是否本地
    bool isLocal() const { return path_.isLocal(); }

    // 获取 Cell（内部使用）
    std::shared_ptr<ActorCell> cell() const { return cell_.lock(); }

    // 设置 Cell（内部使用）
    void setCell(std::shared_ptr<ActorCell> cell) { cell_ = cell; }

    //==========================================================================
    // 消息发送
    //==========================================================================

    // 发送消息（异步，fire-and-forget）
    template<typename T>
    void tell(const T& msg) const {
        Message message(msg, MessageFormat::FlatBuffers);
        tellMessage(message);
    }

    // 发送原始消息
    void tellMessage(const Message& msg) const;

    // 请求-响应（异步，返回 Future）
    template<typename T, typename R>
    std::future<R> ask(const T& msg, int64_t timeoutMs = 5000) const {
        Message message(msg, MessageFormat::FlatBuffers);

        std::promise<R> promise;
        auto future = promise.get_future();

        askMessage(message, timeoutMs, [p = std::move(promise)](const Message& response) mutable {
            try {
                if (response.is<R>()) {
                    p.set_value(response.as<R>());
                } else {
                    p.set_exception(std::make_exception_ptr(
                        std::runtime_error("Unexpected response type")));
                }
            } catch (...) {
                p.set_exception(std::current_exception());
            }
        });

        return future;
    }

    // 请求原始消息
    void askMessage(const Message& msg, int64_t timeoutMs,
                    std::function<void(const Message&)> callback) const;

    // 转发消息（保留原始发送者）
    template<typename T>
    void forward(const T& msg, const ActorRef& sender) const {
        // TODO: 实现
    }

    // 创建远程代理
    ActorRef proxy() const;

    // 比较运算符
    bool operator==(const ActorRef& other) const {
        return path_.toString() == other.path_.toString();
    }

    bool operator!=(const ActorRef& other) const {
        return !(*this == other);
    }

    bool operator<(const ActorRef& other) const {
        return path_.toString() < other.path_.toString();
    }

    // 哈希值（用于容器）
    size_t hash() const {
        return std::hash<std::string>{}(path_.toString());
    }

private:
    ActorPath path_;
    std::weak_ptr<ActorCell> cell_;  // 本地 Actor 单元
    mutable std::shared_ptr<ActorSystem> system_;
};

//==============================================================================
// ActorRef 哈希函数（用于 unordered_map/set）
//==============================================================================

struct ActorRefHash {
    size_t operator()(const ActorRef& ref) const {
        return ref.hash();
    }
};

//==============================================================================
// ActorRef 等价比较（用于 unordered_map/set）
//==============================================================================

struct ActorRefEqual {
    bool operator()(const ActorRef& a, const ActorRef& b) const {
        return a == b;
    }
};

//==============================================================================
// ActorRef 选择器（用于从多个引用中选择）
//==============================================================================

class ActorRefSelector {
public:
    // 选择第一个可用的
    static ActorRef selectFirst(const std::vector<ActorRef>& refs) {
        for (const auto& ref : refs) {
            if (ref.isValid()) {
                return ref;
            }
        }
        return ActorRef{};
    }

    // 随机选择
    static ActorRef selectRandom(const std::vector<ActorRef>& refs) {
        std::vector<ActorRef> validRefs;
        for (const auto& ref : refs) {
            if (ref.isValid()) {
                validRefs.push_back(ref);
            }
        }

        if (validRefs.empty()) {
            return ActorRef{};
        }

        static thread_local std::random_device rd;
        static thread_local std::mt19937 gen(rd());
        std::uniform_int_distribution<size_t> dist(0, validRefs.size() - 1);

        return validRefs[dist(gen)];
    }

    // 选择本地的
    static ActorRef selectLocal(const std::vector<ActorRef>& refs) {
        for (const auto& ref : refs) {
            if (ref.isValid() && ref.isLocal()) {
                return ref;
            }
        }
        return selectFirst(refs);
    }
};

//==============================================================================
// ActorRef 集合
//==============================================================================

class ActorRefSet {
public:
    // 添加引用
    void add(const ActorRef& ref) {
        if (ref.isValid()) {
            refs_.insert(ref);
        }
    }

    // 移除引用
    void remove(const ActorRef& ref) {
        refs_.erase(ref);
    }

    // 获取所有引用
    std::vector<ActorRef> getAll() const {
        return std::vector<ActorRef>(refs_.begin(), refs_.end());
    }

    // 获取本地引用
    std::vector<ActorRef> getLocal() const {
        std::vector<ActorRef> result;
        for (const auto& ref : refs_) {
            if (ref.isLocal()) {
                result.push_back(ref);
            }
        }
        return result;
    }

    // 广播消息
    template<typename T>
    void broadcast(const T& msg) const {
        for (const auto& ref : refs_) {
            ref.tell(msg);
        }
    }

    // 获取大小
    size_t size() const {
        return refs_.size();
    }

    // 是否为空
    bool empty() const {
        return refs_.empty();
    }

private:
    std::unordered_set<ActorRef, ActorRefHash, ActorRefEqual> refs_;
};

} // namespace actor
} // namespace apollo
