#include "processing_encoder/video_provider_adapter.hpp"

#include "capture_video/video_capture_provider.hpp"

namespace piguard::processing_encoder {

VideoProviderAdapter::VideoProviderAdapter(capture_video::VideoCaptureProvider& provider)
    : provider_(provider) {}

void VideoProviderAdapter::register_consumer() {
    consumer_id_ = provider_.register_consumer();
}

void VideoProviderAdapter::unregister_consumer() {
    if (consumer_id_ != 0) {
        provider_.unregister_consumer(consumer_id_);
        consumer_id_ = 0;
    }
}

std::vector<std::shared_ptr<capture_video::VideoFrame>> VideoProviderAdapter::fetch_frames() {
    auto frames = provider_.wait_frame(consumer_id_, last_seq_);
    if (!frames.empty()) {
        last_seq_ = frames.back()->seq;
    }
    return frames;
}

}  // namespace piguard::processing_encoder
