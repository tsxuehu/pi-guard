#pragma once

#include <string>

namespace piguard::infra_log {

class Logger {
public:
    virtual ~Logger() = default;

    virtual void trace(const std::string& message) = 0;
    virtual void debug(const std::string& message) = 0;
    virtual void info(const std::string& message) = 0;
    virtual void warn(const std::string& message) = 0;
    virtual void error(const std::string& message) = 0;

    virtual bool isTraceEnabled() const = 0;
    virtual bool isDebugEnabled() const = 0;
    virtual bool isInfoEnabled() const = 0;
    virtual bool isWarnEnabled() const = 0;
    virtual bool isErrorEnabled() const = 0;
};

}  // namespace piguard::infra_log
