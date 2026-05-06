#include "processing_encoder/encoder.hpp"
#include "infra_log/logger_factory.hpp"
#include "infra_log/logger.hpp"

#include <algorithm>
#include <cstring>
#include <utility>

#include "capture_audio/audio_frame.hpp"
#include "capture_video/video_frame.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/imgutils.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}
namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger = 
    piguard::infra_log::LogFactory::getLogger("Encoder");
}
namespace piguard::processing_encoder {

struct Encoder::VideoCodecContext {
    AVCodecContext* codec_ctx{nullptr};
    AVFrame* frame{nullptr};
    AVPacket* packet{nullptr};
    SwsContext* sws_ctx{nullptr};
    int64_t pts{0};
};

struct Encoder::AudioCodecContext {
    AVCodecContext* codec_ctx{nullptr};
    AVFrame* frame{nullptr};
    AVPacket* packet{nullptr};
    SwrContext* swr_ctx{nullptr};
    int64_t pts{0};
};

Encoder::Encoder(std::shared_ptr<IVideoFrameGetter> video_getter,
                 std::shared_ptr<IAudioFrameGetter> audio_getter,
                 EncoderOptions options)
    : video_getter_(std::move(video_getter)),
      audio_getter_(std::move(audio_getter)),
      options_(options) {}

Encoder::~Encoder() {
    stop();
}

bool Encoder::init_video_encoder() {
    if (!video_getter_) {
        return true;
    }
    video_ctx_ = std::make_unique<VideoCodecContext>();

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (codec == nullptr) {
        logger->error("video encoder not found");
        return false;
    }
    video_ctx_->codec_ctx = avcodec_alloc_context3(codec);
    if (video_ctx_->codec_ctx == nullptr) {
        logger->error("failed to alloc video codec context");
        return false;
    }

    video_ctx_->codec_ctx->width = options_.video_width;
    video_ctx_->codec_ctx->height = options_.video_height;
    video_ctx_->codec_ctx->time_base = AVRational{1, std::max(1, options_.video_fps)};
    video_ctx_->codec_ctx->framerate = AVRational{std::max(1, options_.video_fps), 1};
    video_ctx_->codec_ctx->pix_fmt = AV_PIX_FMT_YUV420P;
    video_ctx_->codec_ctx->bit_rate = options_.video_bitrate;
    video_ctx_->codec_ctx->gop_size = std::max(1, options_.video_fps);
    video_ctx_->codec_ctx->max_b_frames = 0;

    av_opt_set(video_ctx_->codec_ctx->priv_data, "preset", "veryfast", 0);
    av_opt_set(video_ctx_->codec_ctx->priv_data, "tune", "zerolatency", 0);

    if (avcodec_open2(video_ctx_->codec_ctx, codec, nullptr) < 0) {
        logger->error("failed to open video encoder");
        return false;
    }

    video_ctx_->frame = av_frame_alloc();
    video_ctx_->packet = av_packet_alloc();
    if (video_ctx_->frame == nullptr || video_ctx_->packet == nullptr) {
        logger->error("failed to alloc video frame/packet");
        return false;
    }
    video_ctx_->frame->format = video_ctx_->codec_ctx->pix_fmt;
    video_ctx_->frame->width = video_ctx_->codec_ctx->width;
    video_ctx_->frame->height = video_ctx_->codec_ctx->height;
    if (av_frame_get_buffer(video_ctx_->frame, 32) < 0) {
        logger->error("failed to get video frame buffer");
        return false;
    }

    video_ctx_->sws_ctx = sws_getContext(options_.video_width,
                                         options_.video_height,
                                         AV_PIX_FMT_YUYV422,
                                         options_.video_width,
                                         options_.video_height,
                                         AV_PIX_FMT_YUV420P,
                                         SWS_BILINEAR,
                                         nullptr,
                                         nullptr,
                                         nullptr);
    if (video_ctx_->sws_ctx == nullptr) {
        logger->error("failed to create sws context");
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(packet_mtx_);
        video_meta_.ready = true;
        video_meta_.codec_id = video_ctx_->codec_ctx->codec_id;
        video_meta_.time_base_num = video_ctx_->codec_ctx->time_base.num;
        video_meta_.time_base_den = video_ctx_->codec_ctx->time_base.den;
        video_meta_.width = video_ctx_->codec_ctx->width;
        video_meta_.height = video_ctx_->codec_ctx->height;
        video_meta_.extradata.clear();
        if (video_ctx_->codec_ctx->extradata != nullptr && video_ctx_->codec_ctx->extradata_size > 0) {
            video_meta_.extradata.assign(video_ctx_->codec_ctx->extradata,
                                         video_ctx_->codec_ctx->extradata + video_ctx_->codec_ctx->extradata_size);
        }
    }
    logger->debug("video encoder initialized, "
                  + std::to_string(options_.video_width) + "x" + std::to_string(options_.video_height)
                  + " @" + std::to_string(options_.video_fps) + "fps");
    return true;
}

bool Encoder::init_audio_encoder() {
    if (!audio_getter_) {
        return true;
    }
    audio_ctx_ = std::make_unique<AudioCodecContext>();

    const AVCodec* codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    if (codec == nullptr) {
        logger->error("audio encoder not found");
        return false;
    }
    audio_ctx_->codec_ctx = avcodec_alloc_context3(codec);
    if (audio_ctx_->codec_ctx == nullptr) {
        logger->error("failed to alloc audio codec context");
        return false;
    }

    audio_ctx_->codec_ctx->sample_rate = options_.audio_sample_rate;
    audio_ctx_->codec_ctx->bit_rate = options_.audio_bitrate;
    audio_ctx_->codec_ctx->sample_fmt = codec->sample_fmts != nullptr ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    audio_ctx_->codec_ctx->ch_layout = AV_CHANNEL_LAYOUT_MONO;
    if (options_.audio_channels == 2) {
        audio_ctx_->codec_ctx->ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    }
    audio_ctx_->codec_ctx->time_base = AVRational{1, options_.audio_sample_rate};

    if (avcodec_open2(audio_ctx_->codec_ctx, codec, nullptr) < 0) {
        logger->error("failed to open audio encoder");
        return false;
    }

    audio_ctx_->frame = av_frame_alloc();
    audio_ctx_->packet = av_packet_alloc();
    if (audio_ctx_->frame == nullptr || audio_ctx_->packet == nullptr) {
        logger->error("failed to alloc audio frame/packet");
        return false;
    }
    audio_ctx_->frame->format = audio_ctx_->codec_ctx->sample_fmt;
    audio_ctx_->frame->sample_rate = audio_ctx_->codec_ctx->sample_rate;
    audio_ctx_->frame->ch_layout = audio_ctx_->codec_ctx->ch_layout;
    audio_ctx_->frame->nb_samples = audio_ctx_->codec_ctx->frame_size > 0 ? audio_ctx_->codec_ctx->frame_size : 1024;
    if (av_frame_get_buffer(audio_ctx_->frame, 0) < 0) {
        logger->error("failed to get audio frame buffer");
        return false;
    }

    if (audio_ctx_->codec_ctx->sample_fmt != AV_SAMPLE_FMT_S16 &&
        audio_ctx_->codec_ctx->sample_fmt != AV_SAMPLE_FMT_S16P) {
        AVChannelLayout in_layout{};
        av_channel_layout_default(&in_layout, std::max(1, options_.audio_channels));
        swr_alloc_set_opts2(&audio_ctx_->swr_ctx,
                            &audio_ctx_->codec_ctx->ch_layout,
                            audio_ctx_->codec_ctx->sample_fmt,
                            options_.audio_sample_rate,
                            &in_layout,
                            AV_SAMPLE_FMT_S16,
                            options_.audio_sample_rate,
                            0,
                            nullptr);
        if (audio_ctx_->swr_ctx == nullptr || swr_init(audio_ctx_->swr_ctx) < 0) {
            av_channel_layout_uninit(&in_layout);
            logger->error("failed to init swr context");
            return false;
        }
        av_channel_layout_uninit(&in_layout);
    }

    {
        std::lock_guard<std::mutex> lock(packet_mtx_);
        audio_meta_.ready = true;
        audio_meta_.codec_id = audio_ctx_->codec_ctx->codec_id;
        audio_meta_.time_base_num = audio_ctx_->codec_ctx->time_base.num;
        audio_meta_.time_base_den = audio_ctx_->codec_ctx->time_base.den;
        audio_meta_.sample_rate = audio_ctx_->codec_ctx->sample_rate;
        audio_meta_.channels = audio_ctx_->codec_ctx->ch_layout.nb_channels;
        audio_meta_.extradata.clear();
        if (audio_ctx_->codec_ctx->extradata != nullptr && audio_ctx_->codec_ctx->extradata_size > 0) {
            audio_meta_.extradata.assign(audio_ctx_->codec_ctx->extradata,
                                         audio_ctx_->codec_ctx->extradata + audio_ctx_->codec_ctx->extradata_size);
        }
    }
    logger->debug("audio encoder initialized, "
                  + std::to_string(options_.audio_sample_rate) + "Hz "
                  + std::to_string(options_.audio_channels) + "ch, "
                  + "frame_size=" + std::to_string(audio_ctx_->frame->nb_samples));
    return true;
}

void Encoder::close_video_encoder() {
    if (!video_ctx_) {
        return;
    }
    if (video_ctx_->sws_ctx != nullptr) {
        sws_freeContext(video_ctx_->sws_ctx);
    }
    if (video_ctx_->frame != nullptr) {
        av_frame_free(&video_ctx_->frame);
    }
    if (video_ctx_->packet != nullptr) {
        av_packet_free(&video_ctx_->packet);
    }
    if (video_ctx_->codec_ctx != nullptr) {
        avcodec_free_context(&video_ctx_->codec_ctx);
    }
    video_ctx_.reset();
}

void Encoder::close_audio_encoder() {
    if (!audio_ctx_) {
        return;
    }
    if (audio_ctx_->swr_ctx != nullptr) {
        swr_free(&audio_ctx_->swr_ctx);
    }
    if (audio_ctx_->frame != nullptr) {
        av_frame_free(&audio_ctx_->frame);
    }
    if (audio_ctx_->packet != nullptr) {
        av_packet_free(&audio_ctx_->packet);
    }
    if (audio_ctx_->codec_ctx != nullptr) {
        avcodec_free_context(&audio_ctx_->codec_ctx);
    }
    audio_ctx_.reset();
}

bool Encoder::start() {
    if (running_.exchange(true)) {
        logger->debug("encoder already running");
        return true;
    }
    audio_pcm_buf_.clear();
    if (!init_video_encoder() || !init_audio_encoder()) {
        logger->error("encoder init failed");
        running_.store(false);
        close_video_encoder();
        close_audio_encoder();
        return false;
    }

    if (video_getter_) {
        video_thread_ = std::thread(&Encoder::video_encode_loop, this);
    }
    if (audio_getter_) {
        audio_thread_ = std::thread(&Encoder::audio_encode_loop, this);
    }
    logger->info("encoder started");
    return true;
}

void Encoder::flush_video_encoder() {
    if (!video_ctx_ || !video_ctx_->codec_ctx) {
        return;
    }
    avcodec_send_frame(video_ctx_->codec_ctx, nullptr);
    while (avcodec_receive_packet(video_ctx_->codec_ctx, video_ctx_->packet) == 0) {
        auto encoded = std::make_shared<EncodedVideoPacket>();
        encoded->pts = video_ctx_->packet->pts;
        encoded->dts = video_ctx_->packet->dts;
        encoded->key_frame = (video_ctx_->packet->flags & AV_PKT_FLAG_KEY) != 0;
        encoded->data.assign(video_ctx_->packet->data,
                             video_ctx_->packet->data + video_ctx_->packet->size);
        enqueue_packet(std::move(encoded));
        av_packet_unref(video_ctx_->packet);
    }
}

void Encoder::flush_audio_encoder() {
    if (!audio_ctx_ || !audio_ctx_->codec_ctx) {
        return;
    }
    avcodec_send_frame(audio_ctx_->codec_ctx, nullptr);
    while (avcodec_receive_packet(audio_ctx_->codec_ctx, audio_ctx_->packet) == 0) {
        auto encoded = std::make_shared<EncodedAudioPacket>();
        encoded->pts = audio_ctx_->packet->pts;
        encoded->dts = audio_ctx_->packet->dts;
        encoded->data.assign(audio_ctx_->packet->data,
                             audio_ctx_->packet->data + audio_ctx_->packet->size);
        enqueue_packet(std::move(encoded));
        av_packet_unref(audio_ctx_->packet);
    }
}

void Encoder::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    packet_cv_.notify_all();
    if (video_thread_.joinable()) {
        video_thread_.join();
    }
    if (audio_thread_.joinable()) {
        audio_thread_.join();
    }

    flush_video_encoder();
    flush_audio_encoder();
    close_video_encoder();
    close_audio_encoder();
    logger->info("encoder stopped");
}

void Encoder::video_encode_loop() {
    bool first_frame = true;
    while (running_.load(std::memory_order_acquire)) {
        auto frames = video_getter_->fetch_frames();
        if (frames.empty() || !video_ctx_) {
            continue;
        }

        if (frames.size() > 1) {
            logger->debug("video encode loop dropping " + std::to_string(frames.size() - 1)
                          + " frame(s), keeping latest");
        }

        const auto& video_frame = frames.back();
        if (video_frame == nullptr || video_frame->data == nullptr || video_frame->length == 0) {
            continue;
        }

        if (av_frame_make_writable(video_ctx_->frame) < 0) {
            continue;
        }
        uint8_t* src_data[1] = {static_cast<uint8_t*>(video_frame->data)};
        int src_linesize[1] = {options_.video_width * 2};
        sws_scale(video_ctx_->sws_ctx,
                  src_data,
                  src_linesize,
                  0,
                  options_.video_height,
                  video_ctx_->frame->data,
                  video_ctx_->frame->linesize);

        video_ctx_->frame->pts = video_ctx_->pts++;
        if (avcodec_send_frame(video_ctx_->codec_ctx, video_ctx_->frame) < 0) {
            continue;
        }

        while (avcodec_receive_packet(video_ctx_->codec_ctx, video_ctx_->packet) == 0) {
            auto encoded = std::make_shared<EncodedVideoPacket>();
            encoded->pts = video_ctx_->packet->pts;
            encoded->dts = video_ctx_->packet->dts;
            encoded->key_frame = (video_ctx_->packet->flags & AV_PKT_FLAG_KEY) != 0;
            encoded->data.assign(video_ctx_->packet->data,
                                 video_ctx_->packet->data + video_ctx_->packet->size);
            enqueue_packet(std::move(encoded));
            av_packet_unref(video_ctx_->packet);
        }

        if (first_frame) {
            logger->debug("video encode loop: first frame encoded");
            first_frame = false;
        }
    }
}

void Encoder::audio_encode_loop() {
    bool first_frame = true;
    while (running_.load(std::memory_order_acquire)) {
        auto frames = audio_getter_->fetch_frames();
        if (frames.empty() || !audio_ctx_) {
            continue;
        }

        for (const auto& frame : frames) {
            if (frame == nullptr || frame->pcm_data.empty()) {
                continue;
            }
            audio_pcm_buf_.insert(audio_pcm_buf_.end(),
                                  frame->pcm_data.begin(),
                                  frame->pcm_data.end());
        }

        const int channels = std::max(1, options_.audio_channels);
        const int dst_samples = audio_ctx_->frame->nb_samples;
        const int dst_total = dst_samples * channels;

        while (static_cast<int>(audio_pcm_buf_.size()) >= dst_total) {
            if (av_frame_make_writable(audio_ctx_->frame) < 0) {
                break;
            }

            const uint8_t* in_data[1] = {
                reinterpret_cast<const uint8_t*>(audio_pcm_buf_.data())
            };
            if (audio_ctx_->swr_ctx != nullptr) {
                uint8_t** out = audio_ctx_->frame->data;
                swr_convert(audio_ctx_->swr_ctx, out, dst_samples, in_data, dst_samples);
            } else if (audio_ctx_->codec_ctx->sample_fmt == AV_SAMPLE_FMT_S16) {
                std::memcpy(audio_ctx_->frame->data[0],
                            in_data[0],
                            static_cast<size_t>(dst_total) * sizeof(int16_t));
            } else {
                logger->warn("audio encode loop: unsupported sample fmt, clearing buffer");
                audio_pcm_buf_.clear();
                break;
            }

            audio_ctx_->frame->pts = audio_ctx_->pts;
            audio_ctx_->pts += dst_samples;
            if (avcodec_send_frame(audio_ctx_->codec_ctx, audio_ctx_->frame) < 0) {
                logger->warn("audio encode loop: send frame failed, clearing buffer");
                audio_pcm_buf_.clear();
                break;
            }

            while (avcodec_receive_packet(audio_ctx_->codec_ctx, audio_ctx_->packet) == 0) {
                auto encoded = std::make_shared<EncodedAudioPacket>();
                encoded->pts = audio_ctx_->packet->pts;
                encoded->dts = audio_ctx_->packet->dts;
                encoded->data.assign(audio_ctx_->packet->data,
                                     audio_ctx_->packet->data + audio_ctx_->packet->size);
                enqueue_packet(std::move(encoded));
                av_packet_unref(audio_ctx_->packet);
            }

            audio_pcm_buf_.erase(audio_pcm_buf_.begin(),
                                 audio_pcm_buf_.begin() + dst_total);

            if (first_frame) {
                logger->debug("audio encode loop: first frame encoded");
                first_frame = false;
            }
        }
    }
}

void Encoder::enqueue_packet(std::shared_ptr<EncodedPacketBase> packet) {
    std::lock_guard<std::mutex> lock(packet_mtx_);
    packet->seq = ++packet_seq_;
    packet_queue_.push_back({std::move(packet), consumers_});

    while (packet_queue_.size() > options_.packet_queue_capacity) {
        packet_queue_.pop_front();
        logger->warn("packet queue overflow, dropped oldest packet");
    }
    packet_cv_.notify_all();
}

Encoder::consumer_id_t Encoder::register_consumer() {
    std::lock_guard<std::mutex> lock(packet_mtx_);
    const consumer_id_t id = next_consumer_id_++;
    consumers_.insert(id);
    return id;
}

void Encoder::cleanup_consumer_pending_locked(consumer_id_t consumer_id, uint64_t last_seq, bool clear_all) {
    for (auto it = packet_queue_.begin(); it != packet_queue_.end();) {
        const bool should_clear = clear_all ||
            (it->packet->seq > last_seq && it->pending_consumers.count(consumer_id) > 0);
        if (should_clear) {
            it->pending_consumers.erase(consumer_id);
        }
        if (it->pending_consumers.empty()) {
            it = packet_queue_.erase(it);
        } else {
            ++it;
        }
    }
}

void Encoder::unregister_consumer(consumer_id_t consumer_id) {
    std::lock_guard<std::mutex> lock(packet_mtx_);
    consumers_.erase(consumer_id);
    cleanup_consumer_pending_locked(consumer_id, 0, true);
    packet_cv_.notify_all();
}

std::vector<std::shared_ptr<EncodedPacketBase>> Encoder::wait_packet(consumer_id_t consumer_id, uint64_t last_seq) {
    std::unique_lock<std::mutex> lock(packet_mtx_);
    packet_cv_.wait(lock, [this, consumer_id, last_seq] {
        if (!running_.load(std::memory_order_acquire)) {
            return true;
        }
        for (const auto& item : packet_queue_) {
            if (item.packet->seq > last_seq && item.pending_consumers.count(consumer_id) > 0) {
                return true;
            }
        }
        return false;
    });

    if ((!running_.load(std::memory_order_acquire) && packet_queue_.empty()) ||
        consumers_.count(consumer_id) == 0) {
        return {};
    }

    std::vector<std::shared_ptr<EncodedPacketBase>> packets;
    for (const auto& item : packet_queue_) {
        if (item.packet->seq > last_seq && item.pending_consumers.count(consumer_id) > 0) {
            packets.push_back(item.packet);
        }
    }
    if (packets.empty()) {
        return {};
    }
    cleanup_consumer_pending_locked(consumer_id, last_seq, false);
    return packets;
}

EncodedVideoStreamMeta Encoder::video_stream_meta() const {
    std::lock_guard<std::mutex> lock(packet_mtx_);
    return video_meta_;
}

EncodedAudioStreamMeta Encoder::audio_stream_meta() const {
    std::lock_guard<std::mutex> lock(packet_mtx_);
    return audio_meta_;
}

void Encoder::encode_once() {
    if (!running_.load(std::memory_order_acquire)) {
        return;
    }
    // 实时编码由独立线程驱动，此接口保留给兼容旧调用方。
}

}  // namespace piguard::processing_encoder
