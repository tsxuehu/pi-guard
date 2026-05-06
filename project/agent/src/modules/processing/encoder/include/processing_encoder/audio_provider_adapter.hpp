#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "capture_audio/audio_capture_provider.hpp"
#include "processing_encoder/encoder_types.hpp"

namespace piguard::processing_encoder {

class AudioProviderAdapter : public IAudioFrameGetter {
public:
    explicit AudioProviderAdapter(piguard::capture_audio::AudioCaptureProvider& provider);
    ~AudioProviderAdapter() override;

    AudioProviderAdapter(const AudioProviderAdapter&) = delete;
    AudioProviderAdapter& operator=(const AudioProviderAdapter&) = delete;

    std::vector<std::shared_ptr<piguard::capture_audio::AudioFrame>> fetch_frames() override;

private:
    piguard::capture_audio::AudioCaptureProvider& provider_;
    uint32_t consumer_id_{0};
    uint64_t last_seq_{0};
};

}  // namespace piguard::processing_encoder
