#pragma once

#include "apollo/net/session.h"
#include <memory>

namespace apollo {
namespace net {

/**
 * @brief 会话工厂接口
 *
 * 负责创建会话实例
 * 不同服务器类型可以有不同的会话实现
 */
class SessionFactory {
public:
    virtual ~SessionFactory() = default;

    /**
     * @brief 创建会话实例
     * @return 会话指针，失败返回 nullptr
     */
    virtual SessionPtr createSession() = 0;

    /**
     * @brief 销毁会话实例
     */
    virtual void destroySession(Session* session) = 0;

    /**
     * @brief 获取会话类型名称(用于调试)
     */
    virtual const char* getSessionTypeName() const = 0;
};

/**
 * @brief 会话工厂智能指针类型
 */
using SessionFactoryPtr = std::shared_ptr<SessionFactory>;

/**
 * @brief 通用会话工厂模板
 *
 * 简化会话工厂的实现
 * @tparam T 会话类型，必须继承自 Session
 */
template <typename T>
class TemplateSessionFactory : public SessionFactory {
public:
    SessionPtr createSession() override {
        return std::make_shared<T>();
    }

    void destroySession(Session* session) override {
        delete session;
    }

    const char* getSessionTypeName() const override {
        return typeid(T).name();
    }
};

} // namespace net
} // namespace apollo
