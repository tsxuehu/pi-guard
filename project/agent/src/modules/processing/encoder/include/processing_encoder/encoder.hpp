#pragma once

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_set>
#include <vector>

#include "processing_encoder/encoder_types.hpp"

namespace piguard::processing_encoder {

class Encoder {
public:
    using consumer_id_t = uint64_t;

    Encoder(std::shared_ptr<IVideoFrameGetter> video_getter,
            std::shared_ptr<IAudioFrameGetter> audio_getter,
            EncoderOptions options = {});
    ~Encoder();

    Encoder(const Encoder&) = delete;
    Encoder& operator=(const Encoder&) = delete;

    bool start();
    void stop();
    void encode_once();

    consumer_id_t register_consumer();
    void unregister_consumer(consumer_id_t consumer_id);
    std::vector<std::shared_ptr<EncodedPacket>> wait_packet(consumer_id_t consumer_id, uint64_t last_seq);
    EncodedStreamMeta video_stream_meta() const;
    EncodedStreamMeta audio_stream_meta() const;

private:
    struct QueuedPacket {
        std::shared_ptr<EncodedPacket> packet;
        std::unordered_set<consumer_id_t> pending_consumers;
    };

    bool init_video_encoder();
    bool init_audio_encoder();
    void close_video_encoder();
    void close_audio_encoder();
    void video_encode_loop();
    void audio_encode_loop();
    void enqueue_packet(std::shared_ptr<EncodedPacket> packet);
    void flush_video_encoder();
    void flush_audio_encoder();
    void cleanup_consumer_pending_locked(consumer_id_t consumer_id, uint64_t last_seq, bool clear_all);

    std::atomic<bool> running_{false};
    std::shared_ptr<IVideoFrameGetter> video_getter_;
    std::shared_ptr<IAudioFrameGetter> audio_getter_;
    EncoderOptions options_;

    std::thread video_thread_;
    std::thread audio_thread_;

    uint64_t packet_seq_{0};
    consumer_id_t next_consumer_id_{1};
    std::unordered_set<consumer_id_t> consumers_;
    std::deque<QueuedPacket> packet_queue_;
    mutable std::mutex packet_mtx_;
    std::condition_variable packet_cv_;
    EncodedStreamMeta video_meta_;
    EncodedStreamMeta audio_meta_;

    struct VideoCodecContext;
    struct AudioCodecContext;
    std::unique_ptr<VideoCodecContext> video_ctx_;
    std::unique_ptr<AudioCodecContext> audio_ctx_;
};

}  // namespace piguard::processing_encoder
