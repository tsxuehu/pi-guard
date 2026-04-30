#include "frame_viewer_consumer.hpp"

#include "frame_converter.hpp"

#include <opencv2/highgui.hpp>

FrameViewerConsumer::FrameViewerConsumer(std::shared_ptr<piguard::capture_video::VideoCaptureProvider> provider,
                                         std::string consumer_name,
                                         int width,
                                         int height)
    : piguard::capture_video::ConsumerBase(std::move(provider), std::move(consumer_name)),
      width_(width),
      height_(height) {}

void FrameViewerConsumer::process(
    const std::vector<std::shared_ptr<piguard::capture_video::VideoFrame>>& frames) {
    if (frames.empty()) {
        return;
    }

    const auto& frame = frames.back();
    if (frame->data == nullptr || frame->length < static_cast<size_t>(width_ * height_ * 2)) {
        return;
    }

    const auto* yuyv = static_cast<const uint8_t*>(frame->data);
    cv::Mat image = yuyv_to_bgr(yuyv, width_, height_);
    cv::imshow("pi-guard-view-video", image);

    const int key = cv::waitKey(1);
    if (key == 27 || key == 'q' || key == 'Q') {
        stop();
    }
}
