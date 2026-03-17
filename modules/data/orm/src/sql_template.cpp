/**
 * @file sql_template.cpp
 * @brief SqlTemplate 实现 (JDBC 风格)
 */

#include "apollo/database/sql_template.h"
#include <sstream>
#include <algorithm>
#include <mutex>

namespace apollo {
namespace database {

//==============================================================================
// Parameter 实现
//==============================================================================

std::string Parameter::toString() const {
    std::ostringstream oss;
    std::visit([&](const auto& arg) {
        using T = std::decay_t<decltype(arg)>;
        if constexpr (std::is_same_v<T, std::monostate>) {
            oss << "NULL";
        } else if constexpr (std::is_same_v<T, bool>) {
            oss << (arg ? "TRUE" : "FALSE");
        } else if constexpr (std::is_same_v<T, std::string>) {
            oss << "'" << arg << "'";
        } else {
            oss << arg;
        }
    }, value_);
    return oss.str();
}

//==============================================================================
// ResultSet 实现
//==============================================================================

ResultSet::ResultSet(std::vector<std::vector<std::string>> rows,
                     std::vector<std::string> columnNames)
    : rows_(std::move(rows))
    , columnNames_(std::move(columnNames)) {
}

// Row 模板方法实现
template<>
std::string ResultSet::Row::get<std::string>(size_t index) const {
    return index < values.size() ? values[index] : std::string();
}

template<>
int ResultSet::Row::get<int>(size_t index) const {
    return getInt(index);
}

template<>
int64_t ResultSet::Row::get<int64_t>(size_t index) const {
    return getInt64(index);
}

template<>
double ResultSet::Row::get<double>(size_t index) const {
    return getDouble(index);
}

template<>
std::string ResultSet::Row::get<std::string>(const std::string& name) const {
    for (size_t i = 0; i < columnNames.size(); ++i) {
        if (columnNames[i] == name && i < values.size()) {
            return values[i];
        }
    }
    return {};
}

template<>
int ResultSet::Row::get<int>(const std::string& name) const {
    auto str = get<std::string>(name);
    return str.empty() ? 0 : std::stoi(str);
}

template<>
int64_t ResultSet::Row::get<int64_t>(const std::string& name) const {
    auto str = get<std::string>(name);
    return str.empty() ? 0 : std::stoll(str);
}

template<>
double ResultSet::Row::get<double>(const std::string& name) const {
    auto str = get<std::string>(name);
    return str.empty() ? 0.0 : std::stod(str);
}

// 迭代器实现
ResultSet::iterator::iterator(const ResultSet* rs, size_t pos)
    : rs_(rs)
    , pos_(pos) {
    if (pos_ < rs_->rows_.size()) {
        currentRow_.values = rs_->rows_[pos_];
        currentRow_.columnNames = rs_->columnNames_;
    }
}

ResultSet::iterator::reference ResultSet::iterator::operator*() const {
    return currentRow_;
}

ResultSet::iterator::pointer ResultSet::iterator::operator->() const {
    return &currentRow_;
}

ResultSet::iterator& ResultSet::iterator::operator++() {
    ++pos_;
    if (pos_ < rs_->rows_.size()) {
        currentRow_.values = rs_->rows_[pos_];
        currentRow_.columnNames = rs_->columnNames_;
    }
    return *this;
}

ResultSet::iterator ResultSet::iterator::operator++(int) {
    auto tmp = *this;
    ++(*this);
    return tmp;
}

bool ResultSet::iterator::operator==(const iterator& other) const {
    return pos_ == other.pos_;
}

bool ResultSet::iterator::operator!=(const iterator& other) const {
    return !(*this == other);
}

ResultSet::iterator ResultSet::begin() const {
    return iterator(this, 0);
}

ResultSet::iterator ResultSet::end() const {
    return iterator(this, rows_.size());
}

//==============================================================================
// PreparedStatement 实现
//==============================================================================

PreparedStatement::PreparedStatement(std::shared_ptr<IDatabaseConnection> conn,
                                     std::string sql)
    : conn_(std::move(conn))
    , sql_(std::move(sql)) {
    // 预留参数空间 (计算 SQL 中 ? 的数量)
    size_t count = std::count(sql_.begin(), sql_.end(), '?');
    params_.reserve(count);
}

PreparedStatement::~PreparedStatement() = default;

PreparedStatement& PreparedStatement::setBool(size_t index, bool value) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = value;
    return *this;
}

PreparedStatement& PreparedStatement::setInt(size_t index, int32_t value) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = static_cast<int64_t>(value);
    return *this;
}

PreparedStatement& PreparedStatement::setLong(size_t index, int64_t value) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = value;
    return *this;
}

PreparedStatement& PreparedStatement::setDouble(size_t index, double value) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = value;
    return *this;
}

PreparedStatement& PreparedStatement::setString(size_t index, const std::string& value) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = value;
    return *this;
}

PreparedStatement& PreparedStatement::setNull(size_t index) {
    if (index >= params_.size()) {
        params_.resize(index + 1);
    }
    params_[index] = Parameter();
    return *this;
}

std::string PreparedStatement::buildSql() const {
    std::string result;
    result.reserve(sql_.size() * 2);

    size_t paramIndex = 0;
    for (size_t i = 0; i < sql_.size(); ++i) {
        if (sql_[i] == '?') {
            if (paramIndex < params_.size()) {
                // 转义字符串参数
                std::visit([&](const auto& arg) {
                    using T = std::decay_t<decltype(arg)>;
                    if constexpr (std::is_same_v<T, std::monostate>) {
                        result += "NULL";
                    } else if constexpr (std::is_same_v<T, std::string>) {
                        if (conn_) {
                            result += "'" + conn_->escape(arg) + "'";
                        } else {
                            result += "'" + arg + "'";
                        }
                    } else if constexpr (std::is_same_v<T, bool>) {
                        result += arg ? "1" : "0";
                    } else {
                        std::ostringstream oss;
                        oss << arg;
                        result += oss.str();
                    }
                }, params_[paramIndex].get());
                ++paramIndex;
            }
        } else {
            result += sql_[i];
        }
    }
    return result;
}

ResultSet PreparedStatement::query() {
    if (!conn_ || !conn_->isConnected()) {
        return ResultSet({}, {});
    }

    std::string finalSql = buildSql();
    auto rows = conn_->query(finalSql);

    // TODO: 获取列名 (需要数据库元数据支持)
    std::vector<std::string> columnNames;
    if (!rows.empty()) {
        for (size_t i = 0; i < rows[0].size(); ++i) {
            columnNames.push_back("column_" + std::to_string(i));
        }
    }

    return ResultSet(std::move(rows), std::move(columnNames));
}

std::vector<std::map<std::string, std::string>> PreparedStatement::queryForMap() {
    auto rs = query();
    std::vector<std::map<std::string, std::string>> result;

    for (const auto& row : rs) {
        std::map<std::string, std::string> mapRow;
        const auto& colNames = rs.columnNames();
        for (size_t i = 0; i < row.values.size() && i < colNames.size(); ++i) {
            mapRow[colNames[i]] = row.values[i];
        }
        result.push_back(std::move(mapRow));
    }
    return result;
}

int PreparedStatement::executeUpdate() {
    if (!conn_ || !conn_->isConnected()) {
        return -1;
    }

    std::string finalSql = buildSql();
    if (conn_->execute(finalSql)) {
        return static_cast<int>(conn_->rowsAffected());
    }
    return -1;
}

int64_t PreparedStatement::executeInsert() {
    if (!conn_ || !conn_->isConnected()) {
        return -1;
    }

    std::string finalSql = buildSql();
    if (conn_->execute(finalSql)) {
        return conn_->lastInsertId();
    }
    return -1;
}

//==============================================================================
// DatabaseRegistry 实现
//==============================================================================

namespace {
    std::vector<std::unique_ptr<IDatabaseConnectionFactory>>& getFactories() {
        static std::vector<std::unique_ptr<IDatabaseConnectionFactory>> factories;
        return factories;
    }

    std::mutex& getRegistryMutex() {
        static std::mutex mutex;
        return mutex;
    }
}

void DatabaseRegistry::registerFactory(std::unique_ptr<IDatabaseConnectionFactory> factory) {
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    getFactories().push_back(std::move(factory));
}

std::unique_ptr<IDatabaseConnection> DatabaseRegistry::create(DbType type) {
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    for (const auto& factory : getFactories()) {
        if (factory->getType() == type) {
            return factory->create();
        }
    }
    return nullptr;
}

std::unique_ptr<IDatabaseConnection> DatabaseRegistry::create(const std::string& typeStr) {
    DbType type = DbType::MySQL;
    if (typeStr == "mysql" || typeStr == "mysql://") type = DbType::MySQL;
    else if (typeStr == "postgres" || typeStr == "postgresql" || typeStr == "postgres://") type = DbType::PostgreSQL;
    else if (typeStr == "sqlite" || typeStr == "sqlite://") type = DbType::SQLite;
    return create(type);
}

std::vector<DbType> DatabaseRegistry::getSupportedTypes() {
    std::vector<DbType> types;
    std::lock_guard<std::mutex> lock(getRegistryMutex());
    for (const auto& factory : getFactories()) {
        types.push_back(factory->getType());
    }
    return types;
}

bool DatabaseRegistry::isSupported(DbType type) {
    auto types = getSupportedTypes();
    return std::find(types.begin(), types.end(), type) != types.end();
}

void DatabaseRegistry::loadPlugins(const std::string& pluginDir) {
    // TODO: 动态加载插件库
    // Windows: LoadLibrary("apollo_mysql.dll")
    // Linux: dlopen("libapollo_mysql.so")
    (void)pluginDir;
}

//==============================================================================
// SqlTemplate 实现
//==============================================================================

SqlTemplate::SqlTemplate(std::unique_ptr<IDatabaseConnection> conn)
    : conn_(std::move(conn)) {}

SqlTemplate::SqlTemplate(std::shared_ptr<IDatabaseConnection> conn)
    : conn_(std::move(conn)) {}

SqlTemplate::SqlTemplate(const std::string& connectionString)
    : connectionString_(connectionString) {

    // 从连接字符串解析数据库类型并创建连接
    DbType type = DbType::MySQL;
    if (connectionString_.find("sqlite://") == 0) {
        type = DbType::SQLite;
    } else if (connectionString_.find("postgres://") == 0 ||
               connectionString_.find("postgresql://") == 0) {
        type = DbType::PostgreSQL;
    }

    conn_ = DatabaseRegistry::create(type);
    if (conn_) {
        conn_->connect(connectionString_);
    }
}

SqlTemplate::~SqlTemplate() {
    if (conn_) {
        conn_->disconnect();
    }
}

PreparedStatement SqlTemplate::prepare(const std::string& sql) {
    return PreparedStatement(conn_, sql);
}

ResultSet SqlTemplate::query(const std::string& sql) {
    if (!conn_ || !conn_->isConnected()) {
        return ResultSet({}, {});
    }

    auto rows = conn_->query(sql);

    // TODO: 获取列名
    std::vector<std::string> columnNames;
    if (!rows.empty()) {
        for (size_t i = 0; i < rows[0].size(); ++i) {
            columnNames.push_back("column_" + std::to_string(i));
        }
    }

    return ResultSet(std::move(rows), std::move(columnNames));
}

std::vector<std::map<std::string, std::string>> SqlTemplate::queryForMap(
    const std::string& sql) {

    std::vector<std::map<std::string, std::string>> result;
    auto rs = query(sql);

    for (const auto& row : rs) {
        std::map<std::string, std::string> mapRow;
        const auto& colNames = rs.columnNames();
        for (size_t i = 0; i < row.values.size() && i < colNames.size(); ++i) {
            mapRow[colNames[i]] = row.values[i];
        }
        result.push_back(std::move(mapRow));
    }
    return result;
}

std::optional<ResultSet::Row> SqlTemplate::queryForRow(const std::string& sql) {
    auto rs = query(sql);
    auto it = rs.begin();
    if (it != rs.end()) {
        return *it;
    }
    return std::nullopt;
}

std::optional<std::map<std::string, std::string>> SqlTemplate::queryForMapRow(
    const std::string& sql) {

    auto rows = queryForMap(sql);
    if (rows.empty()) {
        return std::nullopt;
    }
    return rows[0];
}

int SqlTemplate::update(const std::string& sql) {
    if (!conn_ || !conn_->isConnected()) {
        return -1;
    }
    if (conn_->execute(sql)) {
        return static_cast<int>(conn_->rowsAffected());
    }
    return -1;
}

int SqlTemplate::insert(const std::string& sql) {
    if (!conn_ || !conn_->isConnected()) {
        return -1;
    }
    if (conn_->execute(sql)) {
        return static_cast<int>(conn_->lastInsertId());
    }
    return -1;
}

void SqlTemplate::begin() {
    if (conn_) {
        conn_->begin();
    }
}

void SqlTemplate::commit() {
    if (conn_) {
        conn_->commit();
    }
}

void SqlTemplate::rollback() {
    if (conn_) {
        conn_->rollback();
    }
}

std::vector<std::string> SqlTemplate::getTableNames() {
    // TODO: 根据数据库类型查询表列表
    return {};
}

std::vector<std::string> SqlTemplate::getColumnNames(const std::string& table) {
    (void)table;
    // TODO: 查询列信息
    return {};
}

bool SqlTemplate::tableExists(const std::string& table) {
    auto tables = getTableNames();
    return std::find(tables.begin(), tables.end(), table) != tables.end();
}

bool SqlTemplate::isConnected() const {
    return conn_ && conn_->isConnected();
}

void SqlTemplate::reconnect() {
    if (conn_) {
        conn_->disconnect();
        conn_->connect(connectionString_);
    }
}

} // namespace database
} // namespace apollo
