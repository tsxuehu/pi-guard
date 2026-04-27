#include "capture_video/video_capture.hpp"

#include "infra_log/logger_factory.hpp"

#include <memory>

namespace piguard::capture_video {

namespace {
const std::shared_ptr<infra_log::Logger> logger = infra_log::LogFactory::getLogger("VideoCapture");
}

VideoCapture::VideoCapture() {}

std::string VideoCapture::name() const { return "VideoCapture"; }

bool VideoCapture::start() {
    return true;
}

void VideoCapture::stop() { }


}  // namespace piguard::capture_video
