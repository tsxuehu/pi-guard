#pragma once

#include <memory>
#include <string>

#include "capture_audio/consumer_base.hpp"
#include "wav_writer.hpp"

/** 将从 Provider 拿到的每一包 PCM 追加进 WAV（与采集节拍一致）。 */
class RecordingConsumer final : public piguard::capture_audio::AudioConsumerBase {
public:
    RecordingConsumer(std::shared_ptr<piguard::capture_audio::AudioCaptureProvider> provider,
                      std::string consumer_name,
                      WavWriter writer);

private:
    void process(const std::vector<std::shared_ptr<piguard::capture_audio::audio_frame>>& frames) override;

    WavWriter writer_;
};
