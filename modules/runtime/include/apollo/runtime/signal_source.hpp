#pragma once

#include "apollo/runtime/application_host.hpp"

#include <initializer_list>
#include <vector>

namespace apollo::runtime {

class ProcessSignalSource final : public ISignalSource {
public:
    ProcessSignalSource();
    explicit ProcessSignalSource(std::initializer_list<int> signals);
    ~ProcessSignalSource() override;

    ProcessSignalSource(const ProcessSignalSource&) = delete;
    ProcessSignalSource& operator=(const ProcessSignalSource&) = delete;

    bool poll(SignalEvent& event) override;

private:
    void install_handlers();
    void uninstall_handlers();

    std::vector<int> signals_;
};

using SignalSource = ProcessSignalSource;

} // namespace apollo::runtime
