#include "srs_rtmp_pusher.hpp"
#include "foundation/shutdown_manager.hpp"

#include <cstring>
#include <stdexcept>

namespace piguard::cli::srs {

SrsRtmpPusher::SrsRtmpPusher(const std::string& rtmp_url,
                            const processing_encoder::EncodedVideoStreamMeta& video_meta,
                            const processing_encoder::EncodedAudioStreamMeta& audio_meta,
                            const std::shared_ptr<piguard::infra_log::Logger>& logger)
    : rtmp_url_(rtmp_url)
    , video_meta_(video_meta)
    , audio_meta_(audio_meta)
    , logger_(logger) {
}

SrsRtmpPusher::~SrsRtmpPusher() {
    stop();
}

void SrsRtmpPusher::start() {
    if (avformat_network_init() < 0) {
        logger_->error("failed to initialize network");
        throw std::runtime_error("Failed to initialize network");
    }

    // 创建输出上下文
    if (avformat_alloc_output_context2(&ofmt_ctx_, nullptr, "flv", rtmp_url_.c_str()) < 0 || ofmt_ctx_ == nullptr) {
        logger_->error("failed to alloc rtmp output context");
        throw std::runtime_error("Failed to alloc rtmp output context");
    }

    // 创建视频流
    vstream_ = avformat_new_stream(ofmt_ctx_, nullptr);
    if (vstream_ == nullptr) {
        logger_->error("failed to create video stream");
        avformat_free_context(ofmt_ctx_);
        throw std::runtime_error("Failed to create video stream");
    }
    
    // 设置视频流参数
    vstream_->time_base = AVRational{video_meta_.time_base_num, video_meta_.time_base_den};
    vstream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vstream_->codecpar->codec_id = static_cast<AVCodecID>(video_meta_.codec_id);
    vstream_->codecpar->width = video_meta_.width;
    vstream_->codecpar->height = video_meta_.height;
    
    if (!video_meta_.extradata.empty()) {
        vstream_->codecpar->extradata_size = static_cast<int>(video_meta_.extradata.size());
        vstream_->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(video_meta_.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(vstream_->codecpar->extradata, video_meta_.extradata.data(), video_meta_.extradata.size());
    }

    // 创建音频流
    astream_ = avformat_new_stream(ofmt_ctx_, nullptr);
    if (astream_ == nullptr) {
        logger_->error("failed to create audio stream");
        avformat_free_context(ofmt_ctx_);
        throw std::runtime_error("Failed to create audio stream");
    }
    
    // 设置音频流参数
    astream_->time_base = AVRational{audio_meta_.time_base_num, audio_meta_.time_base_den};
    astream_->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    astream_->codecpar->codec_id = static_cast<AVCodecID>(audio_meta_.codec_id);
    astream_->codecpar->sample_rate = audio_meta_.sample_rate;
    av_channel_layout_default(&astream_->codecpar->ch_layout, audio_meta_.channels);
    
    if (!audio_meta_.extradata.empty()) {
        astream_->codecpar->extradata_size = static_cast<int>(audio_meta_.extradata.size());
        astream_->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(audio_meta_.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        memcpy(astream_->codecpar->extradata, audio_meta_.extradata.data(), audio_meta_.extradata.size());
    }

    // 打开输出 URL
    if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_->pb, rtmp_url_.c_str(), AVIO_FLAG_WRITE) < 0) {
            logger_->error("failed to open rtmp url: " + rtmp_url_);
            avformat_free_context(ofmt_ctx_);
            throw std::runtime_error("Failed to open rtmp url: " + rtmp_url_);
        }
    }

    // 写入头部
    if (avformat_write_header(ofmt_ctx_, nullptr) < 0) {
        logger_->error("failed to write rtmp header");
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx_->pb);
        }
        avformat_free_context(ofmt_ctx_);
        throw std::runtime_error("Failed to write rtmp header");
    }

    logger_->info("rtmp push started, url=" + rtmp_url_);
    
    running_ = true;
}

void SrsRtmpPusher::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    
    // 写入尾部
    if (ofmt_ctx_) {
        av_write_trailer(ofmt_ctx_);
        
        // 关闭输出文件
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx_->pb);
        }
        
        // 释放上下文
        avformat_free_context(ofmt_ctx_);
        ofmt_ctx_ = nullptr;
    }
    
    
    running_.store(false, std::memory_order_release);
    avformat_network_deinit();
    logger_->info("rtmp push finished");
}


void SrsRtmpPusher::write_packet(const std::shared_ptr<processing_encoder::EncodedPacketBase>& packet) {
    std::lock_guard<std::mutex> lock(writer_mutex_);
    
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    
    AVPacket pkt{};
    pkt.data = const_cast<uint8_t*>(packet->data.data());
    pkt.size = static_cast<int>(packet->data.size());
    pkt.pts = packet->pts;
    pkt.dts = packet->dts;
    
    if (auto video_pkt = std::dynamic_pointer_cast<processing_encoder::EncodedVideoPacket>(packet)) {
        if (video_pkt->key_frame) {
            pkt.flags |= AV_PKT_FLAG_KEY;
        }
        pkt.stream_index = vstream_->index;
        av_packet_rescale_ts(&pkt,
                             AVRational{video_meta_.time_base_num, video_meta_.time_base_den},
                             vstream_->time_base);
    } else {
        pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index = astream_->index;
        av_packet_rescale_ts(&pkt,
                             AVRational{audio_meta_.time_base_num, audio_meta_.time_base_den},
                             astream_->time_base);
    }
    
    int ret = av_interleaved_write_frame(ofmt_ctx_, &pkt);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
        av_strerror(ret, errbuf, sizeof(errbuf));
        logger_->warn(std::string("failed to push one packet: ") + errbuf +
                      " (stream_index=" + std::to_string(pkt.stream_index) +
                      ", pts=" + std::to_string(pkt.pts) + ")");
    }
}

} // namespace piguard::cli::srs