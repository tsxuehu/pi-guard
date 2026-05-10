#pragma once

#include "infra_log/logger.hpp"
#include "processing_encoder/encoder.hpp"

#include <atomic>
#include <memory>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace piguard::cli::srs {

class SrsRtmpPusher {
public:
    explicit SrsRtmpPusher(const std::string& rtmp_url, 
                         const processing_encoder::EncodedVideoStreamMeta& video_meta,
                         const processing_encoder::EncodedAudioStreamMeta& audio_meta,
                         const std::shared_ptr<piguard::infra_log::Logger>& logger);
    
    ~SrsRtmpPusher();
    
    void start();
    void stop();
    
    void write_packet(const std::shared_ptr<processing_encoder::EncodedPacketBase>& packet);

private:
    std::string rtmp_url_;
    AVFormatContext* ofmt_ctx_ = nullptr;
    AVStream* vstream_ = nullptr;
    AVStream* astream_ = nullptr;
    processing_encoder::EncodedVideoStreamMeta video_meta_;
    processing_encoder::EncodedAudioStreamMeta audio_meta_;
    std::shared_ptr<piguard::infra_log::Logger> logger_;
    
    std::atomic<bool> running_{false};
    std::mutex writer_mutex_;

    /// 编码器若输出 AV_NOPTS_VALUE，FLV/RTMP 时间戳会非法；在此用单调值补齐。
    int64_t next_video_pts_{0};
    int64_t next_audio_pts_{0};
};

} // namespace piguard::cli::srs