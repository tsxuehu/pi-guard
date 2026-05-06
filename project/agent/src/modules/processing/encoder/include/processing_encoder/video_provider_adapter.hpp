#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "capture_video/video_capture_provider.hpp"
#include "processing_encoder/encoder_types.hpp"

namespace piguard::processing_encoder {

class VideoProviderAdapter : public IVideoFrameGetter {
public:
    explicit VideoProviderAdapter(piguard::capture_video::VideoCaptureProvider& provider);
    ~VideoProviderAdapter() override;

    VideoProviderAdapter(const VideoProviderAdapter&) = delete;
    VideoProviderAdapter& operator=(const VideoProviderAdapter&) = delete;

    std::vector<std::shared_ptr<piguard::capture_video::VideoFrame>> fetch_frames() override;

private:
    piguard::capture_video::VideoCaptureProvider& provider_;
    uint64_t consumer_id_{0};
    uint64_t last_seq_{0};
};

}  // namespace piguard::processing_encoder
