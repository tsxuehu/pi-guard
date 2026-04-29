#pragma once

#include "audio_capture_provider.hpp"
#include <atomic>
#include <chrono>
#include <memory>
#include <stdexcept>
#include <string_view>

class AudioConsumerBase {
public:
    AudioConsumerBase(std::shared_ptr<AudioCaptureProvider> provider, int target_hz = 0)
        : provider_(std::move(provider)), target_hz_(target_hz) {
        if (target_hz_ < 0) {
            throw std::invalid_argument("target_hz must be >= 0");
        }
        if (provider_) {
            consumer_id_ = provider_->register_consumer();
        }
    }

    virtual ~AudioConsumerBase() {
        if (provider_ && consumer_id_ != 0) {
            provider_->unregister_consumer(consumer_id_);
        }
    }

    void run(std::string_view name) {
        const auto has_rate_limit = target_hz_ > 0;
        const auto process_interval = has_rate_limit
                                          ? std::chrono::milliseconds(1000 / target_hz_)
                                          : std::chrono::milliseconds(0);
        auto next_process_time = std::chrono::steady_clock::now();

        while (running_) {
            auto frame = provider_->wait_audio(consumer_id_, last_seq_);
            if (!frame) {
                break;
            }

            bool should_process = true;
            if (has_rate_limit) {
                const auto now = std::chrono::steady_clock::now();
                if (now < next_process_time) {
                    should_process = false;
                } else {
                    next_process_time = now + process_interval;
                }
            }

            if (should_process) {
                process(name, frame);
            }

            last_seq_ = frame->seq;
        }
    }

    void stop() { running_ = false; }
    virtual void process(std::string_view name, const std::shared_ptr<audio_frame>& frame) = 0;

protected:
    std::shared_ptr<AudioCaptureProvider> provider_;
    AudioCaptureProvider::consumer_id_t consumer_id_{0};
    int target_hz_{0};
    uint64_t last_seq_{0};
    std::atomic<bool> running_{true};
};
