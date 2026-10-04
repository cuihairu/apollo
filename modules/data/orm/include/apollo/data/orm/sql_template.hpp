#pragma once

#include "apollo/data/core/data_source.hpp"

#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace apollo::data::orm {

class SqlTemplate {
public:
    // P0-5（C-95 链接断裂修复）：orm 树的 SqlTemplate 此前只有声明无定义
    // （orm/src/sql_template.cpp 是 legacy apollo::database 树的实现），
    // game-server 链接即断。头内 inline 补齐（经 IDataSource 取连接执行）。
    explicit SqlTemplate(std::shared_ptr<apollo::data::core::IDataSource> data_source)
        : data_source_(std::move(data_source)) {
    }

    apollo::data::core::QueryResult query(const std::string& sql) const {
        if (!data_source_) {
            return {};
        }
        auto connection = data_source_->acquire();
        if (!connection) {
            return {};
        }
        return connection->execute_query(sql);
    }

    apollo::data::core::QueryResult update(const std::string& sql) const {
        if (!data_source_) {
            return {};
        }
        auto connection = data_source_->acquire();
        if (!connection) {
            return {};
        }
        return connection->execute_update(sql);
    }

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
