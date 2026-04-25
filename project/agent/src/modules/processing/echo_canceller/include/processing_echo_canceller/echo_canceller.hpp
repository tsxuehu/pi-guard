#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"

namespace piguard::processing {

class EchoCanceller : public foundation::Module {
public:
    std::string name() const override;
    bool start() override;
    void stop() override;
    void process_once();

private:
    std::atomic<bool> running_{false};
};

}  // namespace piguard::processing
