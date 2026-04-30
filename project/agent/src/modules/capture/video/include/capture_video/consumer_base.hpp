#pragma once

#include "video_capture_provider.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace piguard::capture_video {

class ConsumerBase {
public:
    ConsumerBase(std::shared_ptr<VideoCaptureProvider> provider, std::string consumer_name)
        : provider_(std::move(provider)),
          name_(std::move(consumer_name)) {
        if (provider_) {
            consumer_id_ = provider_->register_consumer();
        }
    }

    virtual ~ConsumerBase() {
        if (provider_) {
            provider_->unregister_consumer(consumer_id_);
        }
    }

    void run() {
        while (running_) {
            auto frames = provider_->wait_frame(consumer_id_, last_seq_);
            if (frames.empty()) break;
            process(frames);
            last_seq_ = frames.back()->seq;
        }
    }

    void stop() { running_ = false; }

    const std::string& name() const { return name_; }

    virtual void process(const std::vector<std::shared_ptr<VideoFrame>>& frames) = 0;

protected:
    std::shared_ptr<VideoCaptureProvider> provider_;
    std::string name_;
    VideoCaptureProvider::consumer_id_t consumer_id_{0};
    uint64_t last_seq_{0};
    std::atomic<bool> running_{true};
};

}  // namespace piguard::capture_video
