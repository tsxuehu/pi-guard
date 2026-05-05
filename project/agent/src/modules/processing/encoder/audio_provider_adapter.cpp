#include "processing_encoder/audio_provider_adapter.hpp"

#include "capture_audio/audio_capture_provider.hpp"

namespace piguard::processing_encoder {

AudioProviderAdapter::AudioProviderAdapter(capture_audio::AudioCaptureProvider& provider)
    : provider_(provider), consumer_id_(provider_.register_consumer()) {}

AudioProviderAdapter::~AudioProviderAdapter() {
    provider_.unregister_consumer(consumer_id_);
}

std::shared_ptr<capture_audio::AudioFrame> AudioProviderAdapter::fetch_next_frame() {
    auto frames = provider_.wait_audio(consumer_id_, last_seq_);
    if (frames.empty()) {
        return nullptr;
    }
    last_seq_ = frames.back()->seq;
    return frames.back();
}

}  // namespace piguard::processing_encoder
