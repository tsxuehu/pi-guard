#pragma once

#include "video_capture_provider.hpp"
#include <string_view>
#include <chrono>
#include <atomic>
#include <stdexcept>

class ConsumerBase {
public:
    ConsumerBase(std::shared_ptr<VideoCaptureProvider> provider, int target_fps)
        : provider_(std::move(provider)), target_fps_(target_fps) {
        if (target_fps_ <= 0) {
            throw std::invalid_argument("target_fps must be > 0");
        }
        if (provider_) {
            consumer_id_ = provider_->register_consumer();
        }
    }

    virtual ~ConsumerBase() {
        if (provider_ && consumer_id_ != 0) {
            provider_->unregister_consumer(consumer_id_);
        }
    }

    void run(std::string_view name) {
        const int source_fps = provider_->capture_fps();
        // 如果 target_fps 为 15，source_fps 为 30，step 为 2，则每两帧处理一帧。
        const int step = (target_fps_ >= source_fps) ? 1 : (source_fps / target_fps_);
        const auto process_interval = std::chrono::milliseconds(1000 / target_fps_);
        auto next_process_time = std::chrono::steady_clock::now();

        while (running_) {
            auto frame = provider_->wait_frame(consumer_id_, last_seq_);
            if (!frame) break;

            bool should_process = true;
            const auto now = std::chrono::steady_clock::now();
            if (now < next_process_time) {
                should_process = false;
            } else {
                next_process_time = now + process_interval;
            }

            // 多个消费者可以获取同一个 frame 引用
            if (should_process && frame->seq % step == 0) {
                process(name, frame);
            }

            last_seq_ = frame->seq;
        }
    }

    void stop() { running_ = false; }
    virtual void process(std::string_view name, const std::shared_ptr<VideoFrame>& frame) = 0;

protected:
    std::shared_ptr<VideoCaptureProvider> provider_;
    VideoCaptureProvider::consumer_id_t consumer_id_{0};
    int target_fps_;
    uint64_t last_seq_{0};
    std::atomic<bool> running_{true};
};
