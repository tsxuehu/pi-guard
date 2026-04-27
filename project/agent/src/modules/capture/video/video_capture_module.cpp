#include "capture_video/video_capture_module.hpp"

#include "infra_log/logger_factory.hpp"

#include <memory>

namespace piguard::capture_video {

namespace {
const std::shared_ptr<infra_log::Logger> logger = infra_log::LogFactory::getLogger("VideoCaptureModule");
}

VideoCaptureModule::VideoCaptureModule() {}

std::string VideoCaptureModule::name() const { return "VideoCaptureModule"; }

bool VideoCaptureModule::start() {
    return true;
}

void VideoCaptureModule::stop() { }


}  // namespace piguard::capture_video
