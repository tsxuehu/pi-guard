#include "capture_audio/audio_capture_provider.hpp"
#include "capture_video/video_capture_provider.hpp"
#include "foundation/shutdown_manager.hpp"
#include "infra_log/logger.hpp"
#include "infra_log/logger_factory.hpp"
#include "processing_encoder/audio_provider_adapter.hpp"
#include "processing_encoder/encoder.hpp"
#include "processing_encoder/video_provider_adapter.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/avutil.h>
}

namespace {
const std::shared_ptr<piguard::infra_log::Logger> logger =
    piguard::infra_log::LogFactory::getLogger("SrsPushDemo");

constexpr int kVideoWidth = 640;
constexpr int kVideoHeight = 480;
constexpr int kVideoFps = 25;
constexpr uint32_t kVideoBufferCount = 4;
constexpr size_t kProviderQueueCapacity = 50;
constexpr unsigned kAudioSampleRate = 16000;
constexpr unsigned kAudioChannels = 1;
constexpr const char* kDefaultVideoDevice = "/dev/video0";
constexpr const char* kDefaultAudioDevice = "hw:1,0";
constexpr const char* kDefaultRtmpUrl = "rtmp://127.0.0.1/live/livestream";
}  // namespace

int main(int argc, char** argv) {
    const std::string video_device = (argc > 1) ? argv[1] : kDefaultVideoDevice;
    const std::string audio_device = (argc > 2) ? argv[2] : kDefaultAudioDevice;
    const std::string rtmp_url = (argc > 3) ? argv[3] : kDefaultRtmpUrl;

    if (std::signal(SIGINT, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR ||
        std::signal(SIGTERM, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR) {
        logger->error("failed to register signal handler");
        return 1;
    }

    avformat_network_init();

    auto video_provider = std::make_shared<piguard::capture_video::VideoCaptureProvider>(
        video_device, kVideoFps, kVideoWidth, kVideoHeight, kVideoBufferCount, kProviderQueueCapacity);
    auto audio_provider = std::make_shared<piguard::capture_audio::AudioCaptureProvider>(
        audio_device, kAudioSampleRate, kAudioChannels);

    auto video_adapter = std::make_shared<piguard::processing_encoder::VideoProviderAdapter>(*video_provider);
    auto audio_adapter = std::make_shared<piguard::processing_encoder::AudioProviderAdapter>(*audio_provider);

    piguard::processing_encoder::EncoderOptions options;
    options.video_width = kVideoWidth;
    options.video_height = kVideoHeight;
    options.video_fps = kVideoFps;
    options.audio_sample_rate = static_cast<int>(kAudioSampleRate);
    options.audio_channels = static_cast<int>(kAudioChannels);

    piguard::processing_encoder::Encoder encoder(video_adapter, audio_adapter, options);

    video_provider->start();
    if (!audio_provider->start()) {
        logger->error("audio provider start failed");
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }
    if (!encoder.start()) {
        logger->error("encoder start failed");
        audio_provider->stop();
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }

    AVFormatContext* ofmt_ctx = nullptr;
    if (avformat_alloc_output_context2(&ofmt_ctx, nullptr, "flv", rtmp_url.c_str()) < 0 || ofmt_ctx == nullptr) {
        logger->error("failed to alloc rtmp output context");
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }

    const auto vmeta = encoder.video_stream_meta();
    const auto ameta = encoder.audio_stream_meta();
    if (!vmeta.ready || !ameta.ready) {
        logger->error("encoder stream meta not ready");
        avformat_free_context(ofmt_ctx);
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }

    AVStream* vstream = avformat_new_stream(ofmt_ctx, nullptr);
    AVStream* astream = avformat_new_stream(ofmt_ctx, nullptr);
    if (vstream == nullptr || astream == nullptr) {
        logger->error("failed to create output streams");
        avformat_free_context(ofmt_ctx);
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }

    vstream->time_base = AVRational{vmeta.time_base_num, vmeta.time_base_den};
    vstream->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vstream->codecpar->codec_id = static_cast<AVCodecID>(vmeta.codec_id);
    vstream->codecpar->width = vmeta.width;
    vstream->codecpar->height = vmeta.height;
    if (!vmeta.extradata.empty()) {
        vstream->codecpar->extradata_size = static_cast<int>(vmeta.extradata.size());
        vstream->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(vmeta.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(vstream->codecpar->extradata, vmeta.extradata.data(), vmeta.extradata.size());
    }

    astream->time_base = AVRational{ameta.time_base_num, ameta.time_base_den};
    astream->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    astream->codecpar->codec_id = static_cast<AVCodecID>(ameta.codec_id);
    astream->codecpar->sample_rate = ameta.sample_rate;
    av_channel_layout_default(&astream->codecpar->ch_layout, ameta.channels);
    if (!ameta.extradata.empty()) {
        astream->codecpar->extradata_size = static_cast<int>(ameta.extradata.size());
        astream->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(ameta.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(astream->codecpar->extradata, ameta.extradata.data(), ameta.extradata.size());
    }

    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&ofmt_ctx->pb, rtmp_url.c_str(), AVIO_FLAG_WRITE) < 0) {
            logger->error("failed to open rtmp url: " + rtmp_url);
            avformat_free_context(ofmt_ctx);
            encoder.stop();
            audio_provider->stop();
            video_provider->stop();
            avformat_network_deinit();
            return 1;
        }
    }

    if (avformat_write_header(ofmt_ctx, nullptr) < 0) {
        logger->error("failed to write rtmp header");
        if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&ofmt_ctx->pb);
        }
        avformat_free_context(ofmt_ctx);
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        avformat_network_deinit();
        return 1;
    }

    logger->info("rtmp push started, url=" + rtmp_url + ", Ctrl+C to stop");
    const auto consumer_id = encoder.register_consumer();
    std::atomic<bool> writer_running{true};

    std::thread writer([&]() {
        uint64_t last_seq = 0;
        while (writer_running.load(std::memory_order_acquire)) {
            auto packets = encoder.wait_packet(consumer_id, last_seq);
            if (packets.empty()) {
                continue;
            }
            for (const auto& item : packets) {
                AVPacket pkt{};
                pkt.data = const_cast<uint8_t*>(item->data.data());
                pkt.size = static_cast<int>(item->data.size());
                pkt.pts = item->pts;
                pkt.dts = item->dts;

                if (auto video_pkt = std::dynamic_pointer_cast<piguard::processing_encoder::EncodedVideoPacket>(item)) {
                    if (video_pkt->key_frame) {
                        pkt.flags |= AV_PKT_FLAG_KEY;
                    }
                    pkt.stream_index = vstream->index;
                    av_packet_rescale_ts(&pkt,
                                         AVRational{vmeta.time_base_num, vmeta.time_base_den},
                                         vstream->time_base);
                } else {
                    pkt.flags |= AV_PKT_FLAG_KEY;
                    pkt.stream_index = astream->index;
                    av_packet_rescale_ts(&pkt,
                                         AVRational{ameta.time_base_num, ameta.time_base_den},
                                         astream->time_base);
                }

                if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
                    logger->warn("failed to push one packet");
                }
                last_seq = item->seq;
            }
        }
    });

    piguard::foundation::ShutdownManager::wait_for_shutdown();
    logger->info("shutdown signal received, stopping...");

    writer_running.store(false, std::memory_order_release);
    encoder.unregister_consumer(consumer_id);
    encoder.stop();
    writer.join();

    av_write_trailer(ofmt_ctx);
    if (!(ofmt_ctx->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&ofmt_ctx->pb);
    }
    avformat_free_context(ofmt_ctx);

    audio_provider->stop();
    video_provider->stop();
    avformat_network_deinit();
    logger->info("rtmp push finished");
    return 0;
}
