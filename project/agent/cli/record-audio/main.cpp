#include "capture_audio/audio_capture_provider.hpp"
#include "recording_consumer.hpp"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

namespace {

// 修改下列常量即可调整录制行为
/** arecord -l: card 0 / device 6 — sof-hda-dsp DMIC */
constexpr const char* kAlsaDevice = "plughw:0,6";
constexpr const char* kOutFileName = "record.wav";  // 相对当前工作目录
constexpr int kRecordSeconds = 5;
constexpr unsigned kSampleRateHz = 16000;
constexpr unsigned kChannels = 1;

}  // namespace

int main() {
    const std::filesystem::path out_path = std::filesystem::current_path() / kOutFileName;

    WavWriter wav;
    if (!wav.open(out_path, kChannels, kSampleRateHz)) {
        std::cerr << "record-audio: 无法创建 " << out_path << '\n';
        return 1;
    }

    auto provider = std::make_shared<AudioCaptureProvider>(
        std::string(kAlsaDevice), kSampleRateHz, kChannels);

    RecordingConsumer consumer(provider, "record-cli", std::move(wav));

    std::thread timer([provider]() {
        std::this_thread::sleep_for(std::chrono::seconds(kRecordSeconds));
        provider->stop();
    });

    if (!provider->start()) {
        std::cerr << "record-audio: start 失败\n";
        return 1;
    }

    consumer.run();
    provider->stop();
    if (timer.joinable()) {
        timer.join();
    }

    std::cerr << "已保存: " << out_path.string() << '\n';
    return 0;
}
