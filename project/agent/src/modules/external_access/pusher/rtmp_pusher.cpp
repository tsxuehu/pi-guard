#include "rtmp_pusher.hpp"

namespace piguard::external_access {

std::string RTMPPusher::name() const { return "RTMPPusher"; }

bool RTMPPusher::start() {
    running_.store(true);
    return true;
}

void RTMPPusher::stop() { running_.store(false); }

void RTMPPusher::push_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::external_access
