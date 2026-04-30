#include "capture_audio/audio_capture_provider.hpp"
#include "recording_consumer.hpp"

#include <atomic>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <pthread.h>
#include <signal.h>
#include <string>
#include <sys/signalfd.h>
#include <thread>
#include <unistd.h>

namespace {

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
            std::cerr << "record-audio: 无法删除已存在文件 " << out_path
                      << ": " << ec.message() << '\n';
            return 1;
        }
    }

    sigset_t sigmask;
    sigemptyset(&sigmask);
    sigaddset(&sigmask, SIGINT);
    sigaddset(&sigmask, SIGTERM);
    if (pthread_sigmask(SIG_BLOCK, &sigmask, nullptr) != 0) {
        std::cerr << "record-audio: pthread_sigmask 失败: " << std::strerror(errno) << '\n';
        return 1;
    }

    const int sigfd = signalfd(-1, &sigmask, SFD_CLOEXEC);
    if (sigfd < 0) {
        std::cerr << "record-audio: signalfd 失败: " << std::strerror(errno) << '\n';
        return 1;
    }

    WavWriter wav;
    if (!wav.open(out_path, kChannels, kSampleRateHz)) {
        std::cerr << "record-audio: 无法创建 " << out_path << '\n';
        (void)close(sigfd);
        return 1;
    }

    auto provider = std::make_shared<piguard::capture_audio::AudioCaptureProvider>(
        std::string(kAlsaDevice), kSampleRateHz, kChannels);

    RecordingConsumer consumer(provider, "record-cli", std::move(wav));

    std::atomic<bool> stop_requested{false};

    auto request_stop = [&]() { stop_requested.store(true, std::memory_order_release); };

    std::thread sig_reader([&]() {
        for (;;) {
            signalfd_siginfo si{};
            const ssize_t n = ::read(sigfd, &si, sizeof(si));
            if (n == static_cast<ssize_t>(sizeof(si))) {
                std::cerr << "record-audio: signal received, requesting stop\n";
                request_stop();
                return;
            }
            if (n < 0 && (errno == EINTR || errno == EAGAIN)) {
                continue;
            }
            return;
        }
    });

    if (!provider->start()) {
        std::cerr << "record-audio: start 失败\n";
        stop_requested.store(true, std::memory_order_release);
        (void)close(sigfd);
        sig_reader.join();
        return 1;
    }

    std::thread capture([&]() { consumer.run(); });

    std::cerr << "录制中… 写入 " << out_path.string()
              << "\nCtrl+C 结束（signalfd 阻塞等待，不写轮询）\n";

    std::cerr << "record-audio: waiting signal thread...\n";
    sig_reader.join();
    std::cerr << "record-audio: signal thread exited\n";
    if (stop_requested.load(std::memory_order_acquire)) {
        std::cerr << "record-audio: calling provider->stop()\n";
        provider->stop();
        std::cerr << "record-audio: provider->stop() returned\n";
    }
    std::cerr << "record-audio: waiting capture thread...\n";
    capture.join();
    std::cerr << "record-audio: capture thread exited\n";

    std::cerr << "已保存: " << out_path.string() << '\n';
    return 0;
}
