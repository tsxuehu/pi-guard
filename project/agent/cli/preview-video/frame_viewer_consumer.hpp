#pragma once

#include <memory>
#include <string>

#include "capture_video/consumer_base.hpp"

class FrameViewerConsumer : public piguard::capture_video::ConsumerBase {
public:
    FrameViewerConsumer(std::shared_ptr<piguard::capture_video::VideoCaptureProvider> provider,
                        std::string consumer_name,
                        int width,
                        int height);

    void process(const std::vector<std::shared_ptr<piguard::capture_video::VideoFrame>>& frames) override;

private:
    int width_{0};
    int height_{0};
};
