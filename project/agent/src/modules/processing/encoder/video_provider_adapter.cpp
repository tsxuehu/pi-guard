#include "processing_encoder/video_provider_adapter.hpp"

#include "capture_video/video_capture_provider.hpp"

namespace piguard::processing_encoder {

VideoProviderAdapter::VideoProviderAdapter(capture_video::VideoCaptureProvider& provider)
    : provider_(provider), consumer_id_(provider_.register_consumer()) {}

VideoProviderAdapter::~VideoProviderAdapter() {
    provider_.unregister_consumer(consumer_id_);
}

std::shared_ptr<capture_video::VideoFrame> VideoProviderAdapter::fetch_next_frame() {
    auto frames = provider_.wait_frame(consumer_id_, last_seq_);
    if (frames.empty()) {
        return nullptr;
    }
    last_seq_ = frames.back()->seq;
    return frames.back();
}

}  // namespace piguard::processing_encoder
