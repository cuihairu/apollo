#include "apollo/data/core/data_source.hpp"
#include "apollo/data/orm/memory_connection.hpp"
#include "apollo/data/orm/sql_template.hpp"

#include <iostream>
#include <memory>
#include <string>

namespace {

bool test_sql_template_query_for_one() {
    auto connection = std::make_shared<apollo::data::orm::MemoryConnection>();
    connection->seed_query(
        "SELECT name, level FROM player WHERE id = 1000",
        {{{"name", "hero"}, {"level", "42"}}});

    auto data_source = std::make_shared<apollo::data::core::SimpleDataSource>(
        [connection]() { return connection; });
    apollo::data::orm::SqlTemplate sql(data_source);

    auto player = sql.query_for_one<std::string>(
        "SELECT name, level FROM player WHERE id = 1000",
        [](const apollo::data::core::QueryRow& row) {
            auto it = row.find("name");
            return it != row.end() ? it->second : std::string{};
        });

    return player.has_value() && *player == "hero";
}

bool test_sql_template_update() {
    auto connection = std::make_shared<apollo::data::orm::MemoryConnection>();
    connection->seed_query("UPDATE player SET online = 1 WHERE id = 1000", {});

    auto data_source = std::make_shared<apollo::data::core::SimpleDataSource>(
        [connection]() { return connection; });
    apollo::data::orm::SqlTemplate sql(data_source);
    auto result = sql.update("UPDATE player SET online = 1 WHERE id = 1000");
    return result.ok;
}

} // namespace

int main() {
    const bool ok =
        test_sql_template_query_for_one() &&
        test_sql_template_update();

    if (!ok) {
        std::cerr << "apollo_data_orm_tests failed" << std::endl;
        return 1;
    }

    std::cout << "apollo_data_orm_tests passed" << std::endl;
    return 0;
}
