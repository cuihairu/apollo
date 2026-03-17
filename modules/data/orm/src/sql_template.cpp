#include "apollo/data/orm/sql_template.hpp"

namespace apollo::data::orm {

SqlTemplate::SqlTemplate(std::shared_ptr<apollo::data::core::IDataSource> data_source)
    : data_source_(std::move(data_source)) {}

apollo::data::core::QueryResult SqlTemplate::query(const std::string& sql) const {
    auto connection = data_source_ ? data_source_->acquire() : nullptr;
    if (!connection) {
        return {.ok = false, .error = "no connection available"};
    }
    if (!connection->is_connected() && !connection->connect()) {
        return {.ok = false, .error = "failed to connect"};
    }
    return connection->execute_query(sql);
}

apollo::data::core::QueryResult SqlTemplate::update(const std::string& sql) const {
    auto connection = data_source_ ? data_source_->acquire() : nullptr;
    if (!connection) {
        return {.ok = false, .error = "no connection available"};
    }
    if (!connection->is_connected() && !connection->connect()) {
        return {.ok = false, .error = "failed to connect"};
    }
    return connection->execute_update(sql);
}

} // namespace apollo::data::orm
