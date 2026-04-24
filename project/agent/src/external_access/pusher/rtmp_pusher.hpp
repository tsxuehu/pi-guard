#pragma once

#include <atomic>
#include <string>

#include "core/module.hpp"

namespace piguard::external_access {

class RTMPPusher : public core::Module {
public:
    std::string name() const override;
    bool start() override;
    void stop() override;
    void push_once();

private:
    std::atomic<bool> running_{false};
};

}  // namespace piguard::external_access
