#include "infra_monitor/perf_monitor.hpp"

namespace piguard::infra {

std::string PerfMonitor::name() const { return "PerfMonitor"; }

bool PerfMonitor::start() {
    running_.store(true);
    return true;
}

void PerfMonitor::stop() { running_.store(false); }

PerfMonitor::Snapshot PerfMonitor::collect() const {
    Snapshot snapshot{};
    if (!running_.load()) {
        return snapshot;
    }
    snapshot.cpu_usage_percent = 18.0F;
    snapshot.memory_usage_percent = 32.0F;
    snapshot.temperature_celsius = 51.0F;
    return snapshot;
}

}  // namespace piguard::infra
