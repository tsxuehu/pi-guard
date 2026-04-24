#include "echo_canceller.hpp"

namespace piguard::processing {

std::string EchoCanceller::name() const { return "EchoCanceller"; }

bool EchoCanceller::start() {
    running_.store(true);
    return true;
}

void EchoCanceller::stop() { running_.store(false); }

void EchoCanceller::process_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::processing
