#include "frame_viewer_consumer.hpp"

#include "frame_converter.hpp"

#include <opencv2/highgui.hpp>

FrameViewerConsumer::FrameViewerConsumer(
    std::shared_ptr<VideoCaptureProvider> provider, int target_fps, int width, int height)
    : ConsumerBase(std::move(provider), target_fps), width_(width), height_(height) {}

void FrameViewerConsumer::process(std::string_view, const std::shared_ptr<VideoFrame>& frame) {
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
