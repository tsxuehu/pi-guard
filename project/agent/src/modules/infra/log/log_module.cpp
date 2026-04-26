#include "infra_log/log_module.hpp"

#include <mutex>

#include "spd_logger.hpp"

namespace piguard::infra_log {

namespace {

std::once_flag g_backend_init_once;

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
struct LogBackendShutdownGuard {
    ~LogBackendShutdownGuard() { detail::shutdownSpdlogBackend(); }
} g_log_backend_shutdown_guard;

}  // namespace

std::string LogModule::name() const { return "LogModule"; }

bool LogModule::start() {
    std::call_once(g_backend_init_once, detail::initSpdlogBackend);
    return true;
}

void LogModule::stop() { detail::shutdownSpdlogBackend(); }

}  // namespace piguard::infra_log
