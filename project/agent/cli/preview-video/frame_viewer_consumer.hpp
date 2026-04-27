#pragma once

#include "capture_video/consumer_base.hpp"

class FrameViewerConsumer : public ConsumerBase {
public:
    FrameViewerConsumer(std::shared_ptr<VideoCaptureProvider> provider, int target_fps, int width, int height);

    void process(std::string_view name, const std::shared_ptr<VideoFrame>& frame) override;

private:
    int width_{0};
    int height_{0};
};
