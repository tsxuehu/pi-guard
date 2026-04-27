#include "frame_viewer_consumer.hpp"

#include <opencv2/highgui.hpp>

#include <iostream>
#include <string>

namespace {
constexpr int kWidth = 640;
constexpr int kHeight = 480;
constexpr int kCaptureFps = 30;
constexpr size_t kProviderQueueCapacity = 30;
constexpr char kWindowName[] = "pi-guard-view-video";
}  // namespace

int main(int argc, char** argv) {
    const std::string device = (argc > 1) ? argv[1] : "/dev/video0";

    try {
        auto provider = std::make_shared<VideoCaptureProvider>(
            device, kCaptureFps, kWidth, kHeight, kProviderQueueCapacity);
        provider->start();

        cv::namedWindow(kWindowName, cv::WINDOW_AUTOSIZE);
        FrameViewerConsumer consumer(provider, kCaptureFps, kWidth, kHeight);
        consumer.run("viewer");

        provider->stop();
    } catch (const std::exception& ex) {
        std::cerr << "view_video error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
