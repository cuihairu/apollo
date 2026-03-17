#pragma once

#include "apollo/data/core/connection.hpp"

#include <functional>
#include <memory>

namespace apollo::data::core {

class IDataSource {
public:
    virtual ~IDataSource() = default;
    virtual ConnectionPtr acquire() = 0;
};

using ConnectionFactory = std::function<ConnectionPtr()>;

class SimpleDataSource final : public IDataSource {
public:
    explicit SimpleDataSource(ConnectionFactory factory)
        : factory_(std::move(factory)) {}

    ConnectionPtr acquire() override {
        return factory_ ? factory_() : ConnectionPtr{};
    }

private:
    ConnectionFactory factory_;
};

} // namespace apollo::data::core
