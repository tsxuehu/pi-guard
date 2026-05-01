#include "recording_consumer.hpp"

RecordingConsumer::RecordingConsumer(std::shared_ptr<piguard::capture_audio::AudioCaptureProvider> provider,
                                     std::string consumer_name,
                                     WavWriter writer)
    : piguard::capture_audio::AudioConsumerBase(std::move(provider), std::move(consumer_name)),
      writer_(std::move(writer)) {}

void RecordingConsumer::process(
    const std::vector<std::shared_ptr<piguard::capture_audio::AudioFrame>>& frames) {
    for (const auto& frame : frames) {
        if (!frame || frame->pcm_data.empty()) {
            continue;
        }
        writer_.write_pcm_s16le(frame->pcm_data.data(), frame->pcm_data.size());
    }
}
