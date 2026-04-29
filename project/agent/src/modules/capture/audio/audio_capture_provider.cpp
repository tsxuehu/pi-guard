#include "capture_audio/audio_capture_provider.hpp"
#include <alsa/asoundlib.h>
#include <chrono>

AudioCaptureProvider::AudioCaptureProvider(const std::string& device)
    : device_(device) {}

AudioCaptureProvider::~AudioCaptureProvider() {
    stop();
}

bool AudioCaptureProvider::start(unsigned int rate, unsigned int channels) {
    if (running_.exchange(true)) return true;

    sample_rate_ = rate;
    channels_ = channels;

    produce_thread_ = std::thread(&AudioCaptureProvider::produce_loop, this);
    return true;
}

void AudioCaptureProvider::stop() {
    if (!running_.exchange(false)) return;

    if (produce_thread_.joinable()) {
        produce_thread_.join();
    }

    std::lock_guard<std::mutex> lock(queue_mtx_);
    queue_.clear();
    cv_.notify_all();
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
        return !running_ || find_latest_frame_locked(id, last_seq) != queue_.end();
    });

    if (!running_) return nullptr;

    auto it = find_latest_frame_locked(id, last_seq);
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
    
    // 使用 SND_PCM_NONBLOCK 结合适当的等待策略或直接阻塞读取
    if (snd_pcm_open(&handle, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0) < 0) {
        running_ = false;
        return;
    }

    // 设置硬件参数
    snd_pcm_set_params(handle, 
                       SND_PCM_FORMAT_S16_LE, 
                       SND_PCM_ACCESS_RW_INTERLEAVED,
                       channels_, 
                       sample_rate_, 
                       1,      
                       50000); 

    const int frame_size = 320; // 20ms @ 16kHz
    std::vector<int16_t> buffer(frame_size * channels_);

    while (running_) {
        int frames = snd_pcm_readi(handle, buffer.data(), frame_size);
        
        if (frames == -EPIPE) {
            snd_pcm_prepare(handle);
            continue;
        } else if (frames < 0) {
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

    snd_pcm_close(handle);
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