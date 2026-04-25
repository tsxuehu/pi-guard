#pragma once

#include <atomic>
#include <string>

#include "foundation/module.hpp"

namespace piguard::output {

class AudioPlayback : public foundation::Module {
public:
    std::string name() const override;
    bool start() override;
    void stop() override;
    void play_once();

private:
    std::atomic<bool> running_{false};
};

}  // namespace piguard::output
