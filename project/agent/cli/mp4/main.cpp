#include "capture_audio/audio_capture_provider.hpp"
#include "capture_video/video_capture_provider.hpp"
#include "foundation/shutdown_manager.hpp"
#include "infra_log/logger.hpp"
#include "infra_log/logger_factory.hpp"
#include "processing_encoder/audio_provider_adapter.hpp"
#include "processing_encoder/encoder.hpp"
#include "processing_encoder/video_provider_adapter.hpp"

#include "mp4_writer.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger =
    piguard::infra_log::LogFactory::getLogger("Mp4RecordDemo");

constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 480;
constexpr int kVideoFps = 25;
constexpr uint32_t kVideoBufferCount = 4;
constexpr size_t kProviderQueueCapacity = 50;
constexpr unsigned kAudioSampleRate = 16000;
constexpr unsigned kAudioChannels = 1;
constexpr const char* kDefaultVideoDevice = "/dev/video0";
/** 默认 ALSA default；按需改 argv[2] 或常量，参见 arecord -l */
constexpr const char* kDefaultAudioDevice = "default";
constexpr const char* kDefaultOutput = ".tmp/demo.mp4";
}  // namespace

int main(int argc, char** argv) {
    const std::string video_device = (argc > 1) ? argv[1] : kDefaultVideoDevice;
    const std::string audio_device = (argc > 2) ? argv[2] : kDefaultAudioDevice;
    const std::string output_file = (argc > 3) ? argv[3] : kDefaultOutput;

    if (std::signal(SIGINT, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR ||
        std::signal(SIGTERM, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR) {
        logger->error("failed to register signal handler");
        return 1;
    }

    auto video_provider = std::make_shared<piguard::capture_video::VideoCaptureProvider>(
        video_device, kVideoFps, kVideoWidth, kVideoHeight, kVideoBufferCount, kProviderQueueCapacity);
    auto audio_provider = std::make_shared<piguard::capture_audio::AudioCaptureProvider>(
        audio_device, kAudioSampleRate, kAudioChannels);

    auto video_adapter = std::make_shared<piguard::processing_encoder::VideoProviderAdapter>(*video_provider);
    auto audio_adapter = std::make_shared<piguard::processing_encoder::AudioProviderAdapter>(*audio_provider);

    piguard::processing_encoder::EncoderOptions options;
    options.video_width = kVideoWidth;
    options.video_height = kVideoHeight;
    options.video_fps = kVideoFps;
    options.audio_sample_rate = static_cast<int>(kAudioSampleRate);
    options.audio_channels = static_cast<int>(kAudioChannels);

    piguard::processing_encoder::Encoder encoder(video_adapter, audio_adapter, options);

    video_provider->start();
    if (!audio_provider->start()) {
        logger->error("audio provider start failed");
        video_provider->stop();
        return 1;
    }
    if (!encoder.start()) {
        logger->error("encoder start failed");
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }

    const auto vmeta = encoder.video_stream_meta();
    const auto ameta = encoder.audio_stream_meta();
    if (!vmeta.ready || !ameta.ready) {
        logger->error("encoder stream meta not ready");
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }

    piguard::Mp4Writer mp4_writer(output_file, vmeta, ameta);
    if (!mp4_writer.write_header()) {
        logger->error("failed to init mp4 writer");
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }

    logger->info("mp4 recording started, output=" + output_file + ", Ctrl+C to stop");
    const auto consumer_id = encoder.register_consumer();
    std::atomic<bool> writer_running{true};

    std::thread writer([&]() {
        uint64_t last_seq = 0;
        while (writer_running.load(std::memory_order_acquire)) {
            auto packets = encoder.wait_packet(consumer_id, last_seq);
            if (packets.empty()) {
                continue;
            }
            for (const auto& item : packets) {
                if (!mp4_writer.write_packet(item)) {
                    logger->warn("failed to write one packet");
                }
                last_seq = item->seq;
            }
        }
    });

    piguard::foundation::ShutdownManager::wait_for_shutdown();
    logger->info("shutdown signal received, stopping...");

    writer_running.store(false, std::memory_order_release);
    encoder.unregister_consumer(consumer_id);
    encoder.stop();
    writer.join();

    mp4_writer.write_trailer();

    audio_provider->stop();
    video_provider->stop();
    logger->info("mp4 recording finished");
    return 0;
}
