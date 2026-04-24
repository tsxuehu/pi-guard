#pragma once

#include <atomic>
#include <string>

#include "module.hpp"

namespace piguard::infra {

class PerfMonitor : public foundation::Module {
public:
    struct Snapshot {
        float cpu_usage_percent{0.0F};
        float memory_usage_percent{0.0F};
        float temperature_celsius{0.0F};
    };

    std::string name() const override;
    bool start() override;
    void stop() override;
    Snapshot collect() const;

private:
    std::atomic<bool> running_{false};
};

}  // namespace piguard::infra
