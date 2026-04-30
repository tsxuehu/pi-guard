#pragma once

#include "audio_capture_provider.hpp"
#include "infra_log/logger_factory.hpp"
#include "infra_log/logger.hpp"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace piguard::capture_audio {
    namespace {
        const std::shared_ptr<infra_log::Logger> logger = infra_log::LogFactory::getLogger("AudioConsumerBase");
    }

/**
 * 与采集线程同频消费：生产者每 enqueue 一包，consumer 从 wait_audio 批量取帧并 process。
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
        bool first_frame_logged = false;
        while (running_) {
            auto frames = provider_->wait_audio(consumer_id_, last_seq_);
            if (frames.empty()) {
                break;
            }

            if (!first_frame_logged) {
                logger->info("consumer '" + name_ + "' received first frame, seq=" +
                             std::to_string(frames.front()->seq));
                first_frame_logged = true;
            }
            process(frames);
            last_seq_ = frames.back()->seq;
        }
    }

    void stop() { running_ = false; }

    /** 仅在构造时给定，生命周期内不变的消费者标识（如线程名 / 日志 tag） */
    const std::string& name() const { return name_; }

    virtual void process(const std::vector<std::shared_ptr<audio_frame>>& frames) = 0;

protected:
    std::shared_ptr<AudioCaptureProvider> provider_;
    std::string name_;
    AudioCaptureProvider::consumer_id_t consumer_id_{0};
    uint64_t last_seq_{0};
    std::atomic<bool> running_{true};
};

}  // namespace piguard::capture_audio
