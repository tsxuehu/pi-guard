#pragma once

#include <memory>
#include <string>

#include "capture_video/consumer_base.hpp"

class FrameViewerConsumer : public ConsumerBase {
public:
    FrameViewerConsumer(std::shared_ptr<VideoCaptureProvider> provider,
                        int target_fps,
                        std::string consumer_name,
                        int width,
                        int height);

    void process(const std::shared_ptr<VideoFrame>& frame) override;

private:
    int width_{0};
    int height_{0};
};
