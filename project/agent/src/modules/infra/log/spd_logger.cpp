#include "spd_logger.hpp"

#include <mutex>
#include <stdexcept>
#include <utility>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

namespace piguard::infra_log::detail {

namespace {

bool g_backend_started = false;
std::mutex g_backend_mutex;

}  // namespace

SpdlogLogger::SpdlogLogger(std::string logger_name) : logger_name_(std::move(logger_name)) {}

void SpdlogLogger::trace(const std::string& message) { get()->trace(message); }
void SpdlogLogger::debug(const std::string& message) { get()->debug(message); }
void SpdlogLogger::info(const std::string& message) { get()->info(message); }
void SpdlogLogger::warn(const std::string& message) { get()->warn(message); }
void SpdlogLogger::error(const std::string& message) { get()->error(message); }

bool SpdlogLogger::isTraceEnabled() const { return get()->should_log(spdlog::level::trace); }
bool SpdlogLogger::isDebugEnabled() const { return get()->should_log(spdlog::level::debug); }
bool SpdlogLogger::isInfoEnabled() const { return get()->should_log(spdlog::level::info); }
bool SpdlogLogger::isWarnEnabled() const { return get()->should_log(spdlog::level::warn); }
bool SpdlogLogger::isErrorEnabled() const { return get()->should_log(spdlog::level::err); }

std::shared_ptr<spdlog::logger> SpdlogLogger::get() const {
    auto logger = spdlog::get(logger_name_);
    if (!logger) {
        logger = spdlog::stdout_color_mt(logger_name_);
    }
    return logger;
}

void initSpdlogBackend() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    if (g_backend_started) {
        return;
    }
    spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
    spdlog::set_level(spdlog::level::trace);
    g_backend_started = true;
}

void shutdownSpdlogBackend() {
    std::lock_guard<std::mutex> lock(g_backend_mutex);
    if (!g_backend_started) {
        return;
    }
    spdlog::shutdown();
    g_backend_started = false;
}

std::shared_ptr<Logger> createSpdLogger(const std::string& name) {
    if (name.empty()) {
        throw std::invalid_argument("logger name must not be empty");
    }
    return std::make_shared<SpdlogLogger>(name);
}

}  // namespace piguard::infra_log::detail
