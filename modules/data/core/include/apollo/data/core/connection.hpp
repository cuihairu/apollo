#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace apollo::data::core {

using QueryRow = std::unordered_map<std::string, std::string>;

struct QueryResult {
    bool ok = true;
    std::string error;
    uint64_t affected_rows = 0;
    std::vector<QueryRow> rows;
};

class IConnection {
public:
    virtual ~IConnection() = default;

    virtual bool connect() = 0;
    virtual void disconnect() = 0;
    virtual bool is_connected() const = 0;
    virtual QueryResult execute_query(const std::string& sql) = 0;
    virtual QueryResult execute_update(const std::string& sql) = 0;
};

using ConnectionPtr = std::shared_ptr<IConnection>;

} // namespace apollo::data::core
