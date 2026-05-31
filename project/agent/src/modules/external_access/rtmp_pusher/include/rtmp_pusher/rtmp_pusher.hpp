#pragma once

#include "encoded_packet_adapter.hpp"
#include "processing_encoder/encoder_types.hpp"

#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace piguard::external_access {

class RtmpPusher {
public:
    explicit RtmpPusher(const std::string& rtmp_url,
                        std::shared_ptr<IEncodedPacketAdapter> adapter);

    ~RtmpPusher();

    void start();
    void stop();

private:
    void run_loop();
    void write_packet(const std::shared_ptr<processing_encoder::EncodedPacketBase>& packet);

    std::string rtmp_url_;
    std::shared_ptr<IEncodedPacketAdapter> adapter_;

    AVFormatContext* ofmt_ctx_ = nullptr;
    AVStream* vstream_ = nullptr;
    AVStream* astream_ = nullptr;
    processing_encoder::EncodedVideoStreamMeta video_meta_;
    processing_encoder::EncodedAudioStreamMeta audio_meta_;

    std::atomic<bool> running_{false};
    std::mutex writer_mutex_;
    std::thread worker_thread_;

    uint64_t last_seq_{0};
    int64_t next_video_pts_{0};
    int64_t next_audio_pts_{0};
};

} // namespace piguard::external_access
