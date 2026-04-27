#include "capture_audio/audio_capture_module.hpp"


namespace piguard::capture {

AudioCapture::AudioCapture(foundation::ThreadSafeQueue<foundation::AudioFrame>& out_queue) : out_queue_(out_queue) {}

std::string AudioCapture::name() const { return "AudioCapture"; }

bool AudioCapture::start() {
   
    return true;
}

void AudioCapture::stop() { }


}  // namespace piguard::capture
