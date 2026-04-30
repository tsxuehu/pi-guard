#include "capture_audio/audio_capture_provider.hpp"

#include "infra_log/logger_factory.hpp"
#include "infra_log/logger.hpp"

#include <alsa/asoundlib.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

namespace piguard::capture_audio {
    namespace {
        const std::shared_ptr<infra_log::Logger> logger = infra_log::LogFactory::getLogger("AudioCaptureProvider");
    }

AudioCaptureProvider::AudioCaptureProvider(std::string device,
                                           unsigned int sample_rate_hz,
                                           unsigned int channels) {
    if (device.empty()) {
        throw std::invalid_argument("AudioCaptureProvider: device must be non-empty");
    }
    if (sample_rate_hz == 0) {
        throw std::invalid_argument("AudioCaptureProvider: sample_rate_hz must be > 0");
    }
    if (channels == 0) {
        throw std::invalid_argument("AudioCaptureProvider: channels must be >= 1");
    }
    device_ = std::move(device);
    sample_rate_ = sample_rate_hz;
    channels_ = channels;
}

AudioCaptureProvider::~AudioCaptureProvider() {
    stop();
}

bool AudioCaptureProvider::start() {
    if (running_.exchange(true)) {
        logger->debug("start called while already running");
        return true;
    }

    logger->info("starting audio capture thread, device=" + device_ +
                 ", sample_rate=" + std::to_string(sample_rate_) +
                 ", channels=" + std::to_string(channels_));
    produce_thread_ = std::thread(&AudioCaptureProvider::produce_loop, this);
    return true;
}

void AudioCaptureProvider::stop() {
    if (!running_.exchange(false)) {
        logger->debug("stop called while already stopped");
        return;
    }
    logger->info("stopping audio capture");

    logger->info("notifying waiters after stop request");

    // TODO: 不等待采集县城结束
    // if (produce_thread_.joinable()) {
    //     logger->info("waiting for capture thread to join");
    //     produce_thread_.join();
    //     logger->info("capture thread joined");
    // }

    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        queue_.clear();
        queue_cv_.notify_all();
    }
    logger->info("audio capture stopped");
}

AudioCaptureProvider::consumer_id_t AudioCaptureProvider::register_consumer() {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    consumer_id_t id = next_consumer_id_++;
    active_consumers_.insert(id);
    logger->debug("registered consumer id=" + std::to_string(id));
    return id;
}

void AudioCaptureProvider::unregister_consumer(consumer_id_t id) {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    active_consumers_.erase(id);
    logger->debug("unregistered consumer id=" + std::to_string(id));
    
    for (auto it = queue_.begin(); it != queue_.end(); ) {
        it->pending_consumers.erase(id);
        if (it->pending_consumers.empty()) {
            it = queue_.erase(it);
        } else {
            ++it;
        }
    }
}

std::shared_ptr<audio_frame> AudioCaptureProvider::wait_audio(consumer_id_t id, uint64_t last_seq) {
    std::unique_lock<std::mutex> lock(queue_mtx_);

    queue_cv_.wait(lock, [this, id, last_seq] {
        return !running_.load(std::memory_order_acquire) ||
               find_latest_frame_locked(id, last_seq) != queue_.end();
    });

    if (!running_.load(std::memory_order_acquire)) {
        return nullptr;
    }

    auto it = find_latest_frame_locked(id, last_seq);
    if (it == queue_.end()) {
        return nullptr;
    }

    auto target_frame = it->frame;

    // 自动清理：标记该消费者已处理当前及之前的所有帧
    auto current = queue_.begin();
    while (current != std::next(it)) {
        current->pending_consumers.erase(id);
        if (current->pending_consumers.empty()) {
            current = queue_.erase(current);
        } else {
            ++current;
        }
    }

    return target_frame;
}

void AudioCaptureProvider::produce_loop() {
    snd_pcm_t* handle = nullptr;
    logger->info("opening ALSA capture device: " + device_);

    const int open_ret = snd_pcm_open(&handle, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (open_ret < 0) {
        logger->error("failed to open ALSA device " + device_ + ": " + snd_strerror(open_ret));
        running_ = false;
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            queue_cv_.notify_all();
        }
        return;
    }

    const int set_ret = snd_pcm_set_params(handle,
                                           SND_PCM_FORMAT_S16_LE,
                                           SND_PCM_ACCESS_RW_INTERLEAVED,
                                           channels_,
                                           sample_rate_,
                                           1,
                                           50000);
    if (set_ret < 0) {
        logger->error("failed to set ALSA params: " + std::string(snd_strerror(set_ret)));
        running_ = false;
        (void)snd_pcm_close(handle);
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            queue_cv_.notify_all();
        }
        return;
    }
    logger->info("ALSA capture initialized");
    const int nonblock_ret = snd_pcm_nonblock(handle, 1);
    if (nonblock_ret < 0) {
        logger->warn("failed to enable nonblock read: " + std::string(snd_strerror(nonblock_ret)));
    } else {
        logger->info("enabled nonblock read mode");
    }

    const unsigned frame_size =
        (sample_rate_ > 0) ? (sample_rate_ * 20u / 1000u) : 320u;
    std::vector<int16_t> buffer(static_cast<size_t>(frame_size) * channels_);
    bool first_frame_logged = false;

    while (running_.load(std::memory_order_acquire)) {
        int frames = snd_pcm_readi(handle, buffer.data(), frame_size);

        if (!running_.load(std::memory_order_acquire)) {
            logger->info("snd_pcm_readi returned after running_ set to false, frames=" + std::to_string(frames));
            break;
        }

        if (frames == -EPIPE) {
            logger->warn("audio overrun (EPIPE), preparing pcm");
            snd_pcm_prepare(handle);
            continue;
        }
        if (frames == -EAGAIN) {
            // 不依赖 ALSA 自己的等待；每 5ms 检查一次 running_，确保 stop 能及时退出
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            continue;
        }
        if (frames < 0) {
            logger->error("snd_pcm_readi failed: " + std::string(snd_strerror(frames)));
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto af = std::make_shared<audio_frame>();
        af->seq = next_seq_++;
        af->pcm_data.assign(buffer.begin(), buffer.begin() + frames * channels_);
        af->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        if (!first_frame_logged) {
            logger->info("first audio frame captured, samples=" +
                         std::to_string(static_cast<size_t>(frames) * channels_));
            first_frame_logged = true;
        }

        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            queue_.push_back({af, active_consumers_});

            if (queue_.size() > max_queue_capacity_) {
                queue_.pop_front();
            }
        }
        queue_cv_.notify_all();
    }
    logger->info("capture loop observed running=false, leaving read loop");

    (void)snd_pcm_close(handle);
    logger->info("audio capture loop exited and pcm closed");
}

std::list<AudioCaptureProvider::queued_audio>::iterator 
AudioCaptureProvider::find_latest_frame_locked(consumer_id_t id, uint64_t last_seq) {
    for (auto it = queue_.rbegin(); it != queue_.rend(); ++it) {
        if (it->frame->seq > last_seq && it->pending_consumers.count(id)) {
            return std::prev(it.base()); 
        }
    }
    return queue_.end();
}

}  // namespace piguard::capture_audio
