#include "capture_video/video_capture.hpp"

#include "infra_log/logger_factory.hpp"

#include <chrono>
#include <memory>

namespace piguard::capture_video {

namespace {
const std::shared_ptr<infra_log::Logger> logger = infra_log::LogFactory::getLogger("VideoCapture");
}

VideoCapture::VideoCapture(foundation::ThreadSafeQueue<foundation::VideoFrame>& out_queue) : out_queue_(out_queue) {}

std::string VideoCapture::name() const { return "VideoCapture"; }

bool VideoCapture::start() {
    running_.store(true);
    return true;
}

void VideoCapture::stop() { running_.store(false); }

void VideoCapture::poll_once() {
    if (!running_.load()) {
        return;
    }
    foundation::VideoFrame frame{};
    frame.pts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
    frame.width = 1920;
    frame.height = 1080;
    out_queue_.push(std::move(frame));
}

}  // namespace piguard::capture_video
