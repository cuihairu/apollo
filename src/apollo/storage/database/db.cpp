/**
 * @file db.cpp
 * @brief 数据库工厂和管理器实现
 */

#include "apollo/storage/database/db.h"
#include "apollo/storage/database/db_mysql.h"
#include <map>

namespace apollo {
namespace storage {
namespace database {

//==============================================================================
// DbFactory 实现
//==============================================================================

IDbConnectionPtr DbFactory::create(const DbConfig& config) {
#ifdef APOLLO_USE_MYSQL_CONNECTOR
    if (config.type == DbType::MySQL) {
        return std::make_shared<MySQLConnection>();
    }
#endif

    // 其他数据库类型可以在这里添加
    // PostgreSQL, SQLite 等

    return nullptr;
}

std::vector<DbType> DbFactory::getAvailableTypes() {
    std::vector<DbType> types;

#ifdef APOLLO_USE_MYSQL_CONNECTOR
    types.push_back(DbType::MySQL);
#endif

    // 添加其他可用类型

    return types;
}

//==============================================================================
// DbManager 实现
//==============================================================================

DbManager& DbManager::instance() {
    static DbManager instance;
    return instance;
}

bool DbManager::initialize(const DbConfig& config) {
    if (connection_) {
        shutdown();
    }

    config_ = config;
    connection_ = DbFactory::create(config);

    if (!connection_) {
        return false;
    }

    return connection_->connect(config);
}

IDbConnectionPtr DbManager::getConnection() {
    return connection_;
}

void DbManager::shutdown() {
    if (connection_) {
        connection_->disconnect();
        connection_.reset();
    }
}

} // namespace database
} // namespace storage
} // namespace apollo
