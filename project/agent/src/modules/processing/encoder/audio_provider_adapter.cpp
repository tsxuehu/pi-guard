#include "processing_encoder/audio_provider_adapter.hpp"

#include "capture_audio/audio_capture_provider.hpp"

namespace piguard::processing_encoder {

AudioProviderAdapter::AudioProviderAdapter(capture_audio::AudioCaptureProvider& provider)
    : provider_(provider), consumer_id_(provider_.register_consumer()) {}

AudioProviderAdapter::~AudioProviderAdapter() {
    provider_.unregister_consumer(consumer_id_);
}

std::vector<std::shared_ptr<capture_audio::AudioFrame>> AudioProviderAdapter::fetch_frames() {
    auto frames = provider_.wait_audio(consumer_id_, last_seq_);
    if (!frames.empty()) {
        last_seq_ = frames.back()->seq;
    }
    return frames;
}

}  // namespace piguard::processing_encoder
