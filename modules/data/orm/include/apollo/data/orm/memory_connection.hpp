#pragma once

#include "apollo/data/core/connection.hpp"

#include <unordered_map>

namespace apollo::data::orm {

class MemoryConnection final : public apollo::data::core::IConnection {
public:
    MemoryConnection() = default;

    bool connect() override;
    void disconnect() override;
    bool is_connected() const override;
    apollo::data::core::QueryResult execute_query(const std::string& sql) override;
    apollo::data::core::QueryResult execute_update(const std::string& sql) override;

    void seed_query(std::string sql, std::vector<apollo::data::core::QueryRow> rows);

private:
    bool connected_ = false;
    std::unordered_map<std::string, std::vector<apollo::data::core::QueryRow>> rows_by_sql_;
};

} // namespace apollo::data::orm
