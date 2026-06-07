#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"

namespace piguard::external_access {

class RTMPPusherModule : public foundation::Module {
public:
    std::string name() const override;
    bool start() override;
    void stop() override;

};

}  // namespace piguard::external_access
