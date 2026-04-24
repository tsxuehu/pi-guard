#include "audio_capture.hpp"

#include <chrono>

namespace piguard::capture {

AudioCapture::AudioCapture(foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue) : out_queue_(out_queue) {}

std::string AudioCapture::name() const { return "AudioCapture"; }

bool AudioCapture::start() {
    running_.store(true);
    return true;
}

void AudioCapture::stop() { running_.store(false); }

void AudioCapture::poll_once() {
    if (!running_.load()) {
        return;
    }
    foundation::AudioFrame frame{};
    frame.pts_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now().time_since_epoch())
                       .count();
    frame.sample_rate = 16000;
    frame.channels = 1;
    frame.samples.resize(320);
    out_queue_.push(std::move(frame));
}

}  // namespace piguard::capture
