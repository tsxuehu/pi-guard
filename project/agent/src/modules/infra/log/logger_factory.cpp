#include "infra_log/logger_factory.hpp"
#include "spd_logger.hpp"

namespace piguard::infra_log {

std::shared_ptr<Logger> LogFactory::getLogger(const std::string& name) {
    return detail::createSpdLogger(name);
}

}  // namespace piguard::infra_log
