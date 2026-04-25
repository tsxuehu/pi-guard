#include "output_audio/audio_playback.hpp"

namespace piguard::output {

std::string AudioPlayback::name() const { return "AudioPlayback"; }

bool AudioPlayback::start() {
    running_.store(true);
    return true;
}

void AudioPlayback::stop() { running_.store(false); }

void AudioPlayback::play_once() {
    if (!running_.load()) {
        return;
    }
}

}  // namespace piguard::output
