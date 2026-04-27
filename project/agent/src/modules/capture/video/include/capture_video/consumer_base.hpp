#pragma once

#include "video_capture_provider.hpp"
#include <string_view>
#include <chrono>
#include <thread>
#include <atomic>

class ConsumerBase {
public:
    ConsumerBase(std::shared_ptr<VideoCaptureProvider> provider, int target_fps)
        : provider_(std::move(provider)), target_fps_(target_fps) {
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
        // 假设源帧率为 30fps，计算步长。
        // 如果 target_fps 为 15，step 为 2，则每两帧处理一帧。
        const int step = (target_fps_ >= 30 || target_fps_ <= 0) ? 1 : (30 / target_fps_);

        while (running_) {
            auto frame = provider_->wait_frame(consumer_id_, last_seq_);
            if (!frame) break;

            // 多个消费者可以获取同一个 frame 引用
            if (frame->seq % step == 0) {
                process(name, frame);
            }

            last_seq_ = frame->seq;

            // 通过控制消费间隔产生反压，使消费者自动跳到最新帧
            if (target_fps_ > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(1000 / target_fps_));
            }
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
