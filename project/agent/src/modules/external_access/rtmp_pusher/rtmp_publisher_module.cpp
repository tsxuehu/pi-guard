#include "rtmp_pusher/rtmp_publisher_module.hpp"

namespace piguard::external_access {

std::string RTMPPusherModule::name() const { return "RTMPPusherModule"; }

bool RTMPPusherModule::start() {
    running_.store(true);
    return true;
}

void RTMPPusherModule::stop() { running_.store(false); }

void RTMPPusherModule::push_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::external_access
