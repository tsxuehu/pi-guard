#pragma once

#include <string>

#include "core/module.hpp"

namespace piguard::infra {

class Logger : public core::Module {
public:
    enum class Level { Debug, Info, Warn, Error, Fatal };

    std::string name() const override;
    bool start() override;
    void stop() override;
    void log(Level level, const std::string& message) const;
};

}  // namespace piguard::infra
