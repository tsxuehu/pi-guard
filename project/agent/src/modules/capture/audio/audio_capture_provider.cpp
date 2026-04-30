#include "capture_audio/audio_capture_provider.hpp"

#include <alsa/asoundlib.h>

#include <chrono>
#include <stdexcept>
#include <string>
#include <utility>

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
    if (running_.exchange(true)) return true;

    produce_thread_ = std::thread(&AudioCaptureProvider::produce_loop, this);
    return true;
}

void AudioCaptureProvider::stop() {
    if (!running_.exchange(false)) return;

    // 先在采集 PCM 上 drop，打断可能仍为阻塞或非阻塞链路里卡住的 read
    {
        std::lock_guard<std::mutex> pcm_lk(pcm_drop_mtx_);
        if (pcm_for_drop_ != nullptr) {
            (void)snd_pcm_drop(static_cast<snd_pcm_t*>(pcm_for_drop_));
        }
    }

    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        cv_.notify_all();
    }

    if (produce_thread_.joinable()) {
        produce_thread_.join();
    }

    {
        std::lock_guard<std::mutex> lock(queue_mtx_);
        queue_.clear();
        cv_.notify_all();
    }
}

AudioCaptureProvider::consumer_id_t AudioCaptureProvider::register_consumer() {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    consumer_id_t id = next_consumer_id_++;
    active_consumers_.insert(id);
    return id;
}

void AudioCaptureProvider::unregister_consumer(consumer_id_t id) {
    std::lock_guard<std::mutex> lock(queue_mtx_);
    active_consumers_.erase(id);
    
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

    cv_.wait(lock, [this, id, last_seq] {
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

    if (snd_pcm_open(&handle, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
        running_ = false;
        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            cv_.notify_all();
        }
        return;
    }

    snd_pcm_set_params(handle, 
                       SND_PCM_FORMAT_S16_LE, 
                       SND_PCM_ACCESS_RW_INTERLEAVED,
                       channels_, 
                       sample_rate_, 
                       1,      
                       50000); 

    (void)snd_pcm_nonblock(handle, 1);

    {
        std::lock_guard<std::mutex> pcm_lk(pcm_drop_mtx_);
        pcm_for_drop_ = handle;
    }

    const unsigned frame_size =
        (sample_rate_ > 0) ? (sample_rate_ * 20u / 1000u) : 320u;
    std::vector<int16_t> buffer(static_cast<size_t>(frame_size) * channels_);

    while (running_.load(std::memory_order_acquire)) {
        int frames = snd_pcm_readi(handle, buffer.data(), frame_size);

        if (frames == -EPIPE) {
            snd_pcm_prepare(handle);
            continue;
        }
        if (frames == -EAGAIN) {
            (void)snd_pcm_wait(handle, 20);
            continue;
        }
        if (frames < 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }

        auto af = std::make_shared<audio_frame>();
        af->seq = next_seq_++;
        af->pcm_data.assign(buffer.begin(), buffer.begin() + frames * channels_);
        af->timestamp = std::chrono::steady_clock::now().time_since_epoch().count();

        {
            std::lock_guard<std::mutex> lock(queue_mtx_);
            queue_.push_back({af, active_consumers_});

            if (queue_.size() > max_queue_capacity_) {
                queue_.pop_front();
            }
        }
        cv_.notify_all();
    }

    {
        std::lock_guard<std::mutex> pcm_lk(pcm_drop_mtx_);
        pcm_for_drop_ = nullptr;
    }
    (void)snd_pcm_close(handle);
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
