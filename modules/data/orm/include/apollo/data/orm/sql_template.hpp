#pragma once

#include "apollo/data/core/data_source.hpp"

#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace apollo::data::orm {

class SqlTemplate {
public:
    explicit SqlTemplate(std::shared_ptr<apollo::data::core::IDataSource> data_source);

    apollo::data::core::QueryResult query(const std::string& sql) const;
    apollo::data::core::QueryResult update(const std::string& sql) const;

    template <typename T>
    std::vector<T> query(const std::string& sql,
                         const std::function<T(const apollo::data::core::QueryRow&)>& mapper) const {
        auto result = query(sql);
        std::vector<T> out;
        if (!result.ok) {
            return out;
        }
        out.reserve(result.rows.size());
        for (const auto& row : result.rows) {
            out.push_back(mapper(row));
        }
        return out;
    }

    template <typename T>
    std::optional<T> query_for_one(
        const std::string& sql,
        const std::function<T(const apollo::data::core::QueryRow&)>& mapper) const {
        auto values = query<T>(sql, mapper);
        if (values.empty()) {
            return std::nullopt;
        }
        return values.front();
    }

private:
    std::shared_ptr<apollo::data::core::IDataSource> data_source_;
};

} // namespace apollo::data::orm
