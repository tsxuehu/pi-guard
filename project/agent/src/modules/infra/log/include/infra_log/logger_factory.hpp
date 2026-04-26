#pragma once

#include <memory>
#include <string>

#include "infra_log/logger.hpp"

namespace piguard::infra_log {

class LogFactory {
public:
    static std::shared_ptr<Logger> getLogger(const std::string& name);
};

}  // namespace piguard::infra_log
