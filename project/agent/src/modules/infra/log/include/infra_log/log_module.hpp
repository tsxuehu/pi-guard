#pragma once

#include <string>

#include "foundation/module.hpp"

namespace piguard::infra_log {

class LogModule : public foundation::Module {
public:
    std::string name() const override;
    bool start() override;
    void stop() override;
};

}  // namespace piguard::infra_log
