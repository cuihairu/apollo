#include "apollo/data/orm/memory_connection.hpp"

namespace apollo::data::orm {

bool MemoryConnection::connect() {
    connected_ = true;
    return true;
}

void MemoryConnection::disconnect() {
    connected_ = false;
}

bool MemoryConnection::is_connected() const {
    return connected_;
}

apollo::data::core::QueryResult MemoryConnection::execute_query(const std::string& sql) {
    if (!connected_) {
        return {.ok = false, .error = "memory connection is not connected"};
    }

    auto it = rows_by_sql_.find(sql);
    if (it == rows_by_sql_.end()) {
        return {.ok = true, .rows = {}};
    }

    return {.ok = true, .affected_rows = static_cast<uint64_t>(it->second.size()), .rows = it->second};
}

apollo::data::core::QueryResult MemoryConnection::execute_update(const std::string& sql) {
    if (!connected_) {
        return {.ok = false, .error = "memory connection is not connected"};
    }

    auto result = execute_query(sql);
    result.rows.clear();
    return result;
}

void MemoryConnection::seed_query(std::string sql, std::vector<apollo::data::core::QueryRow> rows) {
    rows_by_sql_[std::move(sql)] = std::move(rows);
}

} // namespace apollo::data::orm
