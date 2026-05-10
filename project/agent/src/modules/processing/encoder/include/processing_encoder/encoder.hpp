#pragma once

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

    void start();
    void stop();
    void encode_once();

    consumer_id_t register_consumer();
    void unregister_consumer(consumer_id_t consumer_id);
    std::vector<std::shared_ptr<EncodedPacketBase>> wait_packet(consumer_id_t consumer_id, uint64_t last_seq);
    EncodedVideoStreamMeta video_stream_meta() const;
    EncodedAudioStreamMeta audio_stream_meta() const;

private:
    struct QueuedPacket {
        std::shared_ptr<EncodedPacketBase> packet;
        std::unordered_set<consumer_id_t> pending_consumers;
    };

    bool init_video_encoder();
    bool init_audio_encoder();
    void close_video_encoder();
    void close_audio_encoder();
    void video_encode_loop();
    void audio_encode_loop();
    void enqueue_packet(std::shared_ptr<EncodedPacketBase> packet);
    void flush_video_encoder();
    void flush_audio_encoder();
    void cleanup_consumer_pending_locked(consumer_id_t consumer_id, uint64_t last_seq, bool clear_all);
    // 在 state_mtx_ 下置 running_=false 并唤醒等待者；返回此前是否处于运行态。
    bool mark_stopped();

    std::shared_ptr<IVideoFrameGetter> video_getter_;
    std::shared_ptr<IAudioFrameGetter> audio_getter_;
    EncoderOptions options_;

    std::thread video_thread_;
    std::thread audio_thread_;

    // state_mtx_ 保护下方所有共享状态：running_/packet_queue_/consumers_ 等。
    // state_cv_ 在「有新包可取」或「停止」时被通知，谓词读到的状态与本锁一致。
    mutable std::mutex state_mtx_;
    std::condition_variable state_cv_;

    bool running_{false};
    uint64_t packet_seq_{0};
    consumer_id_t next_consumer_id_{1};
    std::unordered_set<consumer_id_t> consumers_;
    std::deque<QueuedPacket> packet_queue_;
    EncodedVideoStreamMeta video_meta_;
    EncodedAudioStreamMeta audio_meta_;

    struct VideoCodecContext;
    struct AudioCodecContext;
    std::unique_ptr<VideoCodecContext> video_ctx_;
    std::unique_ptr<AudioCodecContext> audio_ctx_;

    std::vector<int16_t> audio_pcm_buf_;
    /// Encoder::start 时打点，音视频 PTS 均相对此时刻换算到各自 codec time_base
    uint64_t program_t0_ns_{0};
    bool audio_pts_base_initialized_{false};
    uint64_t audio_pcm_front_epoch_ns_{0};
};

}  // namespace piguard::processing_encoder
