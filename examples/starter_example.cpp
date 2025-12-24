/**
 * @file starter_example.cpp
 * @brief Apollo Starter 自动配置示例
 *
 * 演示如何使用类似 Spring Boot Starter 的自动配置机制
 */

#include "apollo/starter/ApolloApplication.h"
#include "apollo/starter/Starter.h"
#include <iostream>

using namespace Apollo::Starter;

/**
 * @brief 基础示例：使用默认配置启动应用
 *
 * 所有默认启用的 Starter 将被自动加载：
 * - ConfigStarter (配置管理)
 * - LoggingStarter (日志系统)
 * - NetworkStarter (网络通信)
 * - GameStarter (游戏系统)
 */
void basicExample() {
    std::cout << "=== 基础示例：默认配置 ===" << std::endl;

    auto app = ApolloApplication::Builder()
        .build();

    std::cout << "激活的 Starter 数量: " << app->getActiveStarters().size() << std::endl;
    for (const auto& starter : app->getActiveStarters()) {
        std::cout << "  - " << starter->getMetadata().name
                  << ": " << starter->getMetadata().description << std::endl;
    }

    app->start();

    // 应用运行逻辑...

    app->stop();
}

/**
 * @brief 高级示例：自定义配置
 *
 * 演示如何：
 * - 启用/禁用特定 Starter
 * - 设置配置属性
 * - 使用配置文件
 */
void advancedExample() {
    std::cout << "\n=== 高级示例：自定义配置 ===" << std::endl;

    auto app = ApolloApplication::Builder()
        // 启用存储组件（默认禁用）
        .enableStarter("storage-starter")
        // 设置配置属性
        .withProperty("storage.database.enabled", "true")
        .withProperty("storage.database.url", "postgresql://localhost/mydb")
        .withProperty("storage.redis.enabled", "true")
        .withProperty("storage.redis.host", "localhost")
        .withProperty("storage.redis.port", "6379")
        // 设置日志级别
        .withProperty("logging.level", "DEBUG")
        .build();

    std::cout << "激活的 Starter 数量: " << app->getActiveStarters().size() << std::endl;

    app->start();

    // 应用运行逻辑...

    app->stop();
}

/**
 * @brief 条件装配示例
 *
 * 演示如何通过配置控制 Starter 的启用
 */
void conditionalExample() {
    std::cout << "\n=== 条件装配示例 ===" << std::endl;

    // 场景 1: 禁用游戏系统
    std::cout << "场景 1: 禁用游戏系统" << std::endl;
    auto app1 = ApolloApplication::Builder()
        .withProperty("game.enabled", "false")
        .build();

    std::cout << "激活的 Starter: ";
    for (const auto& starter : app1->getActiveStarters()) {
        std::cout << starter->getMetadata().name << " ";
    }
    std::cout << std::endl;

    // 场景 2: 只启用网络和日志
    std::cout << "场景 2: 只启用网络和日志" << std::endl;
    auto app2 = ApolloApplication::Builder()
        .enableStarter("network-starter")
        .enableStarter("logging-starter")
        .withProperty("game.enabled", "false")
        .build();

    std::cout << "激活的 Starter: ";
    for (const auto& starter : app2->getActiveStarters()) {
        std::cout << starter->getMetadata().name << " ";
    }
    std::cout << std::endl;
}

/**
 * @brief 优先级示例
 *
 * 演示 Starter 的加载顺序
 */
void priorityExample() {
    std::cout << "\n=== 优先级示例 ===" << std::endl;

    auto app = ApolloApplication::Builder().build();

    std::cout << "Starter 加载顺序（按优先级）:" << std::endl;
    for (const auto& starter : app->getActiveStarters()) {
        std::cout << "  [" << starter->getMetadata().order << "] "
                  << starter->getMetadata().name << std::endl;
    }
}

int main() {
    std::cout << "Apollo Starter 自动配置示例" << std::endl;
    std::cout << "==============================" << std::endl;

    try {
        basicExample();
        advancedExample();
        conditionalExample();
        priorityExample();

        std::cout << "\n所有示例执行成功！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
}
