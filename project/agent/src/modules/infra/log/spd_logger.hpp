#pragma once

#include <memory>
#include <string>

#include "infra_log/logger.hpp"
#include <spdlog/spdlog.h>

namespace piguard::infra_log::detail {

class SpdlogLogger final : public Logger {
public:
    explicit SpdlogLogger(std::string logger_name);

    void trace(const std::string& message) override;
    void debug(const std::string& message) override;
    void info(const std::string& message) override;
    void warn(const std::string& message) override;
    void error(const std::string& message) override;

    bool isTraceEnabled() const override;
    bool isDebugEnabled() const override;
    bool isInfoEnabled() const override;
    bool isWarnEnabled() const override;
    bool isErrorEnabled() const override;

private:
    std::shared_ptr<spdlog::logger> get() const;

    std::string logger_name_;
};

void initSpdlogBackend();
void shutdownSpdlogBackend();
std::shared_ptr<Logger> createSpdLogger(const std::string& name);

}  // namespace piguard::infra_log::detail
