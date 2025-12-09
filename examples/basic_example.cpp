#include "apollo/BaseComponent.h"
#include "apollo/ComponentRegistry.h"
#include "apollo/ApplicationContext.h"
#include <iostream>

using namespace Apollo;

class DatabaseService : public BaseComponent {
public:
    DatabaseService() : BaseComponent("DatabaseService") {}

    DECLARE_COMPONENT(DatabaseService, "DatabaseService")

    bool onInitialize() override {
        std::cout << "DatabaseService: Initializing database connection..." << std::endl;
        return true;
    }

    bool onStart() override {
        std::cout << "DatabaseService: Starting database service..." << std::endl;
        return true;
    }

    bool onStop() override {
        std::cout << "DatabaseService: Stopping database service..." << std::endl;
        return true;
    }

    void executeQuery(const std::string& query) {
        std::cout << "DatabaseService: Executing query: " << query << std::endl;
    }
};

class LoggingService : public BaseComponent {
public:
    LoggingService() : BaseComponent("LoggingService") {}

    DECLARE_COMPONENT(LoggingService, "LoggingService")

    bool onInitialize() override {
        std::cout << "LoggingService: Initializing logging system..." << std::endl;
        return true;
    }

    bool onStart() override {
        std::cout << "LoggingService: Starting logging service..." << std::endl;
        return true;
    }

    void log(const std::string& message) {
        std::cout << "[LOG] " << message << std::endl;
    }
};

class UserService : public BaseComponent {
public:
    UserService() : BaseComponent("UserService") {
        addDependency("DatabaseService");
        addDependency("LoggingService");
    }

    DECLARE_COMPONENT(UserService, "UserService")

    bool onInitialize() override {
        auto db = getComponent<DatabaseService>();
        auto logger = getComponent<LoggingService>();

        if (!db || !logger) {
            std::cerr << "UserService: Failed to get dependencies" << std::endl;
            return false;
        }

        logger->log("UserService: Initializing user service...");
        return true;
    }

    bool onStart() override {
        auto logger = getComponent<LoggingService>();
        logger->log("UserService: Starting user service...");
        return true;
    }

    void createUser(const std::string& username) {
        auto db = getComponent<DatabaseService>();
        auto logger = getComponent<LoggingService>();

        logger->log("UserService: Creating user " + username);
        db->executeQuery("INSERT INTO users (name) VALUES ('" + username + "')");
    }
};

REGISTER_COMPONENT(DatabaseService)
REGISTER_COMPONENT(LoggingService)
REGISTER_COMPONENT(UserService)

int main() {
    auto& context = ApplicationContext::getInstance();

    std::cout << "=== Apollo IoC Framework Demo ===" << std::endl;
    std::cout << "\nRegistered components:" << std::endl;
    for (const auto& name : context.getComponentNames()) {
        std::cout << "  - " << name << std::endl;
    }

    std::cout << "\n=== Initializing Components ===" << std::endl;
    if (!context.initializeComponents()) {
        std::cerr << "Failed to initialize components!" << std::endl;
        return 1;
    }

    std::cout << "\n=== Starting Components ===" << std::endl;
    if (!context.startComponents()) {
        std::cerr << "Failed to start components!" << std::endl;
        return 1;
    }

    std::cout << "\n=== Application Running ===" << std::endl;
    auto userService = context.getComponent<UserService>();
    if (userService) {
        userService->createUser("Alice");
        userService->createUser("Bob");
    }

    std::cout << "\n=== Stopping Components ===" << std::endl;
    context.stopComponents();

    std::cout << "\n=== Destroying Components ===" << std::endl;
    context.destroyComponents();

    std::cout << "\nDone!" << std::endl;
    return 0;
}