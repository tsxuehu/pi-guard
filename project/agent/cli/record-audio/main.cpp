#include "capture_audio/audio_capture_provider.hpp"
#include "recording_consumer.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <iostream>
#include <memory>
#include <mutex>
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
constexpr int kRecordSeconds = 5;
constexpr unsigned kSampleRateHz = 16000;
constexpr unsigned kChannels = 1;

}  // namespace

int main() {
    const std::filesystem::path out_path = std::filesystem::current_path() / kOutFileName;

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

    auto provider = std::make_shared<AudioCaptureProvider>(
        std::string(kAlsaDevice), kSampleRateHz, kChannels);

    RecordingConsumer consumer(provider, "record-cli", std::move(wav));

    std::mutex stop_mtx;
    std::condition_variable stop_cv;
    std::atomic<bool> shutting_down{false};

    auto request_stop = [&]() {
        const bool first = !shutting_down.exchange(true);
        if (first) {
            provider->stop();
        }
        stop_cv.notify_all();
    };

    std::thread capture([&]() { consumer.run(); });

    std::thread timer([&]() {
        std::unique_lock<std::mutex> lk(stop_mtx);
        const auto deadline =
            std::chrono::steady_clock::now() + std::chrono::seconds(kRecordSeconds);
        const bool stopped_early = stop_cv.wait_until(
            lk,
            deadline,
            [&]() { return shutting_down.load(std::memory_order_acquire); });
        lk.unlock();
        // 若为超时（非提前停机），再走一次停机；已与 SIG 停机去重。
        if (!stopped_early) {
            request_stop();
        }
    });

    std::thread sig_reader([&]() {
        for (;;) {
            signalfd_siginfo si{};
            const ssize_t n = ::read(sigfd, &si, sizeof(si));
            if (n == static_cast<ssize_t>(sizeof(si))) {
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
        request_stop();
        (void)close(sigfd);
        capture.join();
        timer.join();
        sig_reader.join();
        return 1;
    }

    std::cerr << "录制中… 写入 " << out_path.string()
              << "\nCtrl+C 结束（signalfd 阻塞等待，不写轮询）；最长 " << kRecordSeconds << " s\n";

    capture.join();

    (void)close(sigfd);  // 唤醒阻塞在 read 的信号线程以便退出
    sig_reader.join();
    timer.join();

    provider->stop();

    std::cerr << "已保存: " << out_path.string() << '\n';
    return 0;
}
