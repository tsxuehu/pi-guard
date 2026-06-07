#include "rtmp_pusher/rtmp_publisher_module.hpp"

namespace piguard::external_access {

std::string RTMPPusherModule::name() const { return "RTMPPusherModule"; }

bool RTMPPusherModule::start() {
    return true;
}

void RTMPPusherModule::stop() {  }


}  // namespace piguard::external_access
