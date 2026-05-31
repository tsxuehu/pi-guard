#include "capture_audio/audio_capture_provider.hpp"
#include "capture_video/video_capture_provider.hpp"
#include "foundation/shutdown_manager.hpp"
#include "infra_log/logger.hpp"
#include "infra_log/logger_factory.hpp"
#include "processing_encoder/audio_provider_adapter.hpp"
#include "processing_encoder/encoder.hpp"
#include "processing_encoder/video_provider_adapter.hpp"
#include "rtmp_pusher/encoder_adapter.hpp"
#include "rtmp_pusher/rtmp_pusher.hpp"
#include "rtmp_pusher_config.hpp"

#include <csignal>
#include <cstdint>
#include <memory>
#include <string>

const auto logger = piguard::infra_log::LogFactory::getLogger("RtmpPusherDemo");

int main(int argc, char** argv) {
    const piguard::cli::rtmp_pusher::Config config;

    // 解析命令行参数
    const std::string video_device = (argc > 1) ? argv[1] : config.default_video_device;
    const std::string audio_device = (argc > 2) ? argv[2] : config.default_audio_device;
    const std::string rtmp_url = (argc > 3) ? argv[3] : config.default_rtmp_url;

    // 注册信号处理器
    if (std::signal(SIGINT, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR ||
        std::signal(SIGTERM, piguard::foundation::ShutdownManager::handle_signal) == SIG_ERR) {
        logger->error("failed to register signal handler");
        return 1;
    }

    // 创建音视频捕获器
    auto video_provider = std::make_shared<piguard::capture_video::VideoCaptureProvider>(
        video_device, config.video_fps, config.video_width, config.video_height,
        config.video_buffer_count, config.provider_queue_capacity);
    auto audio_provider = std::make_shared<piguard::capture_audio::AudioCaptureProvider>(
        audio_device, config.audio_sample_rate, config.audio_channels);

    // 创建适配器
    auto video_adapter = std::make_shared<piguard::processing_encoder::VideoProviderAdapter>(*video_provider);
    auto audio_adapter = std::make_shared<piguard::processing_encoder::AudioProviderAdapter>(*audio_provider);

    // 配置编码器选项
    piguard::processing_encoder::EncoderOptions encoder_options;
    encoder_options.video_width = config.video_width;
    encoder_options.video_height = config.video_height;
    encoder_options.video_fps = config.video_fps;
    encoder_options.audio_sample_rate = static_cast<int>(config.audio_sample_rate);
    encoder_options.audio_channels = static_cast<int>(config.audio_channels);

    // 创建编码器
    piguard::processing_encoder::Encoder encoder(video_adapter, audio_adapter, encoder_options);

    // 启动音视频捕获
    video_provider->start();
    audio_provider->start();

    // 启动编码器
    encoder.start();

    // 创建 EncoderAdapter 并启动 RTMP 推流
    auto encoder_adapter = std::make_shared<piguard::external_access::EncoderAdapter>(encoder);
    piguard::external_access::RtmpPusher rtmp_pusher(rtmp_url, encoder_adapter);

    try {
        rtmp_pusher.start();
    } catch (const std::exception& e) {
        logger->error("failed to start rtmp pusher: " + std::string(e.what()));
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }

    // 等待关闭信号
    piguard::foundation::ShutdownManager::wait_for_shutdown();
    logger->info("shutdown signal received, stopping...");

    // 停止所有组件
    rtmp_pusher.stop();
    encoder.stop();
    audio_provider->stop();
    video_provider->stop();

    logger->info("rtmp push finished");
    return 0;
}
