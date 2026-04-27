#include "frame_viewer_consumer.hpp"
#include "v4l2_capture_session.hpp"

#include <opencv2/highgui.hpp>

#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const std::string device = (argc > 1) ? argv[1] : "/dev/video0";
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr uint32_t kBufferCount = 4;

    try {
        V4L2CaptureSession session({
            .device = device,
            .width = kWidth,
            .height = kHeight,
            .buffer_count = kBufferCount,
        });

        auto provider = std::make_shared<VideoCaptureProvider>(session.fd(), 30);
        provider->set_mmap_buffers(session.buffer_addrs());
        provider->start();

        cv::namedWindow("pi-guard-view-video", cv::WINDOW_AUTOSIZE);
        FrameViewerConsumer consumer(provider, 30, kWidth, kHeight);
        consumer.run("viewer");

        provider->stop();
    } catch (const std::exception& ex) {
        std::cerr << "view_video error: " << ex.what() << std::endl;
        return 1;
    }

    return 0;
}
