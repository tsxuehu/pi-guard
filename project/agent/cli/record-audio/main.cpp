#include "capture_audio/audio_capture_provider.hpp"
#include "infra_log/logger_factory.hpp"
#include "infra_log/logger.hpp"
#include "recording_consumer.hpp"
#include "shutdown_manager.hpp"

#include <cerrno>
#include <csignal>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>

namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger = 
    piguard::infra_log::LogFactory::getLogger("RecordAudioCli");

/** arecord -l: card 0 / device 6 — sof-hda-dsp DMIC */
constexpr const char* kAlsaDevice = "plughw:0,6";
constexpr const char* kOutFileName = ".tmp/record.wav";
constexpr unsigned kSampleRateHz = 16000;
constexpr unsigned kChannels = 1;

}  // namespace

int main() {
    const std::filesystem::path out_path = std::filesystem::current_path() / kOutFileName;
    if (std::filesystem::exists(out_path)) {
        std::error_code ec;
        if (!std::filesystem::remove(out_path, ec) || ec) {
            logger->error("unable to remove existing output file " + out_path.string() +
                          ": " + ec.message());
            return 1;
        }
    }

    if (std::signal(SIGINT, ShutdownManager::handle_signal) == SIG_ERR ||
        std::signal(SIGTERM, ShutdownManager::handle_signal) == SIG_ERR) {
        logger->error("failed to register signal handler");
        return 1;
    }

    WavWriter wav;
    if (!wav.open(out_path, kChannels, kSampleRateHz)) {
        logger->error("failed to create output wav: " + out_path.string());
        return 1;
    }

    auto provider = std::make_shared<piguard::capture_audio::AudioCaptureProvider>(
        std::string(kAlsaDevice), kSampleRateHz, kChannels);

    RecordingConsumer consumer(provider, "record-cli", std::move(wav));

    if (!provider->start()) {
        logger->error("provider start failed");
        return 1;
    }

    std::thread capture([&]() { consumer.run(); });

    logger->info("recording started, output=" + out_path.string() +
                 ", press Ctrl+C to stop");

    logger->info("waiting stop signal");
    ShutdownManager::wait_for_shutdown();

    logger->info("signal received, calling provider stop");
    provider->stop();
    logger->info("provider exit");

    logger->info("waiting capture thread");
    capture.join();
    logger->info("capture thread exited");

    logger->info("recording finished, saved to " + out_path.string());
    return 0;
}
