#include "infra_log/logger.hpp"

#include <iostream>

namespace piguard::infra {

std::string Logger::name() const { return "Logger"; }

bool Logger::start() { return true; }

void Logger::stop() {}

void Logger::log(Level level, const std::string& message) const {
    static const char* kText[] = {"DEBUG", "INFO", "WARN", "ERROR", "FATAL"};
    std::cout << "[" << kText[static_cast<int>(level)] << "] " << message << '\n';
}

}  // namespace piguard::infra
