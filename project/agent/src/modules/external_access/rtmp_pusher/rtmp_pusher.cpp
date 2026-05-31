#include "rtmp_pusher/rtmp_pusher.hpp"
#include "infra_log/logger_factory.hpp"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger =
    piguard::infra_log::LogFactory::getLogger("RtmpPusher");
}

namespace piguard::external_access {

RtmpPusher::RtmpPusher(const std::string& rtmp_url,
                       std::shared_ptr<IEncodedPacketAdapter> adapter)
    : rtmp_url_(rtmp_url)
    , adapter_(std::move(adapter)) {
}

RtmpPusher::~RtmpPusher() {
    stop();
}

void RtmpPusher::start() {
    if (!adapter_) {
        throw std::runtime_error("RtmpPusher: adapter is null");
    }

    if (avformat_network_init() < 0) {
        logger->error("failed to initialize network");
        throw std::runtime_error("Failed to initialize network");
    }

    if (avformat_alloc_output_context2(&ofmt_ctx_, nullptr, "flv", rtmp_url_.c_str()) < 0 || ofmt_ctx_ == nullptr) {
        logger->error("failed to alloc rtmp output context");
        throw std::runtime_error("Failed to alloc rtmp output context");
    }

    video_meta_ = adapter_->video_stream_meta();
    audio_meta_ = adapter_->audio_stream_meta();

    if (video_meta_.ready) {
        vstream_ = avformat_new_stream(ofmt_ctx_, nullptr);
        if (vstream_ == nullptr) {
            logger->error("failed to create video stream");
            avformat_free_context(ofmt_ctx_);
            ofmt_ctx_ = nullptr;
            throw std::runtime_error("Failed to create video stream");
        }
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
    }

    if (audio_meta_.ready) {
        astream_ = avformat_new_stream(ofmt_ctx_, nullptr);
        if (astream_ == nullptr) {
            logger->error("failed to create audio stream");
            avformat_free_context(ofmt_ctx_);
            ofmt_ctx_ = nullptr;
            throw std::runtime_error("Failed to create audio stream");
        }
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
    }

    if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx_->pb, rtmp_url_.c_str(), AVIO_FLAG_WRITE) < 0) {
            logger->error("failed to open rtmp url: " + rtmp_url_);
            avformat_free_context(ofmt_ctx_);
            ofmt_ctx_ = nullptr;
            throw std::runtime_error("Failed to open rtmp url: " + rtmp_url_);
        }
    }

    if (avformat_write_header(ofmt_ctx_, nullptr) < 0) {
        logger->error("failed to write rtmp header");
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx_->pb);
        }
        avformat_free_context(ofmt_ctx_);
        ofmt_ctx_ = nullptr;
        throw std::runtime_error("Failed to write rtmp header");
    }

    logger->info("rtmp push started, url=" + rtmp_url_);

    next_video_pts_ = 0;
    next_audio_pts_ = 0;
    running_ = true;
    last_seq_ = 0;

    if (adapter_) {
        adapter_->register_consumer();
    }

    worker_thread_ = std::thread(&RtmpPusher::run_loop, this);
}

void RtmpPusher::stop() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    if (adapter_) {
        adapter_->unregister_consumer();
    }

    if (ofmt_ctx_) {
        av_write_trailer(ofmt_ctx_);
        if (!(ofmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx_->pb);
        }
        avformat_free_context(ofmt_ctx_);
        ofmt_ctx_ = nullptr;
    }

    vstream_ = nullptr;
    astream_ = nullptr;
    running_.store(false, std::memory_order_release);
    avformat_network_deinit();
    logger->info("rtmp push finished");
}

void RtmpPusher::run_loop() {
    while (running_.load(std::memory_order_acquire)) {
        if (!adapter_) {
            break;
        }

        auto packets = adapter_->wait_packet(last_seq_);
        if (packets.empty()) {
            continue;
        }

        uint64_t batch_max_seq = last_seq_;
        for (const auto& packet : packets) {
            write_packet(packet);
            batch_max_seq = std::max(batch_max_seq, packet->seq);
        }
        last_seq_ = batch_max_seq;
    }
}

void RtmpPusher::write_packet(const std::shared_ptr<processing_encoder::EncodedPacketBase>& packet) {
    std::lock_guard<std::mutex> lock(writer_mutex_);

    if (!running_.load(std::memory_order_acquire)) {
        return;
    }

    AVPacket pkt{};
    pkt.data = const_cast<uint8_t*>(packet->data.data());
    pkt.size = static_cast<int>(packet->data.size());

    if (auto video_pkt = std::dynamic_pointer_cast<processing_encoder::EncodedVideoPacket>(packet)) {
        if (!vstream_) {
            return;
        }
        int64_t pts = packet->pts;
        int64_t dts = packet->dts;
        if (pts == AV_NOPTS_VALUE) {
            pts = next_video_pts_;
        }
        if (dts == AV_NOPTS_VALUE) {
            dts = pts;
        }
        next_video_pts_ = std::max(next_video_pts_, pts + 1);
        pkt.pts = pts;
        pkt.dts = dts;
        if (video_pkt->key_frame) {
            pkt.flags |= AV_PKT_FLAG_KEY;
        }
        pkt.stream_index = vstream_->index;
        av_packet_rescale_ts(&pkt,
                             AVRational{video_meta_.time_base_num, video_meta_.time_base_den},
                             vstream_->time_base);
    } else {
        if (!astream_) {
            return;
        }
        int64_t pts = packet->pts;
        int64_t dts = packet->dts;
        if (pts == AV_NOPTS_VALUE) {
            pts = next_audio_pts_;
        }
        if (dts == AV_NOPTS_VALUE) {
            dts = pts;
        }
        constexpr int k_default_aac_frame_samples = 1024;
        next_audio_pts_ = std::max(next_audio_pts_, pts + k_default_aac_frame_samples);
        pkt.pts = pts;
        pkt.dts = dts;
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
        logger->warn(std::string("failed to push one packet: ") + errbuf +
                      " (stream_index=" + std::to_string(pkt.stream_index) +
                      ", pts=" + std::to_string(pkt.pts) + ")");
    }
}

} // namespace piguard::external_access
