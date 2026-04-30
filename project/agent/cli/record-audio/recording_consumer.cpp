#include "recording_consumer.hpp"

RecordingConsumer::RecordingConsumer(std::shared_ptr<AudioCaptureProvider> provider,
                                     std::string consumer_name,
                                     WavWriter writer)
    : AudioConsumerBase(std::move(provider), std::move(consumer_name)),
      writer_(std::move(writer)) {}

void RecordingConsumer::process(const std::shared_ptr<audio_frame>& frame) {
    if (!frame || frame->pcm_data.empty()) {
        return;
    }
    writer_.write_pcm_s16le(frame->pcm_data.data(), frame->pcm_data.size());
}
