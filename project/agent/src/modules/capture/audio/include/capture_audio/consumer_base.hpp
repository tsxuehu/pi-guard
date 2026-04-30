#pragma once

#include "audio_capture_provider.hpp"
#include <atomic>
#include <memory>
#include <string>

namespace piguard::capture_audio {

/**
 * 与采集线程同频消费：生产者每 enqueue 一包，consumer 即从 wait_audio 取到并 process。
 */
class AudioConsumerBase {
public:
    AudioConsumerBase(std::shared_ptr<AudioCaptureProvider> provider, std::string consumer_name)
        : provider_(std::move(provider)), name_(std::move(consumer_name)) {
        if (provider_) {
            consumer_id_ = provider_->register_consumer();
        }
    }

    virtual ~AudioConsumerBase() {
        if (provider_) {
            provider_->unregister_consumer(consumer_id_);
        }
    }

    void run() {
        while (running_) {
            auto frame = provider_->wait_audio(consumer_id_, last_seq_);
            if (!frame) {
                break;
            }

            process(frame);
            last_seq_ = frame->seq;
        }
    }

    void stop() { running_ = false; }

    /** 仅在构造时给定，生命周期内不变的消费者标识（如线程名 / 日志 tag） */
    const std::string& name() const { return name_; }

    virtual void process(const std::shared_ptr<audio_frame>& frame) = 0;

protected:
    std::shared_ptr<AudioCaptureProvider> provider_;
    std::string name_;
    AudioCaptureProvider::consumer_id_t consumer_id_{0};
    uint64_t last_seq_{0};
    std::atomic<bool> running_{true};
};

}  // namespace piguard::capture_audio
