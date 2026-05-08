#include "capture_audio/audio_capture_provider.hpp"
#include "capture_video/video_capture_provider.hpp"
#include "foundation/shutdown_manager.hpp"
#include "infra_log/logger.hpp"
#include "infra_log/logger_factory.hpp"
#include "processing_encoder/audio_provider_adapter.hpp"
#include "processing_encoder/encoder.hpp"
#include "processing_encoder/video_provider_adapter.hpp"
#include "srs_config.hpp"
#include "srs_rtmp_pusher.hpp"

#include <atomic>
#include <csignal>
#include <cstdint>
#include <cstring>
#include <memory>
#include <string>
#include <thread>

 // 获取日志器
const auto logger = piguard::infra_log::LogFactory::getLogger("SrsPushDemo");

int main(int argc, char** argv) {
    const piguard::cli::srs::Config config;
    
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
    
    // 等待编码器元数据就绪
    const auto vmeta = encoder.video_stream_meta();
    const auto ameta = encoder.audio_stream_meta();
    if (!vmeta.ready || !ameta.ready) {
        logger->error("encoder stream meta not ready");
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }
    
    // 创建 RTMP 推流器
    piguard::cli::srs::SrsRtmpPusher rtmp_pusher(rtmp_url, vmeta, ameta, logger);
    
    // 启动 RTMP 推流
    try {
        rtmp_pusher.start();
    } catch (const std::exception& e) {
        logger->error("failed to start rtmp pusher: " + std::string(e.what()));
        encoder.stop();
        audio_provider->stop();
        video_provider->stop();
        return 1;
    }
    
    // 注册消费者
    const auto consumer_id = encoder.register_consumer();
    
    // 创建数据消费线程
    std::atomic<bool> consumer_running{true};
    std::thread consumer_thread([&]() {
        uint64_t last_seq = 0;
        while (consumer_running.load(std::memory_order_acquire)) {
            auto packets = encoder.wait_packet(consumer_id, last_seq);
            if (packets.empty()) {
                continue;
            }
            for (const auto& packet : packets) {
                rtmp_pusher.write_packet(packet);
            }
        }
    });
    
    // 等待关闭信号
    piguard::foundation::ShutdownManager::wait_for_shutdown();
    logger->info("shutdown signal received, stopping...");
    
    // 停止所有组件
    consumer_running.store(false, std::memory_order_release);
    encoder.unregister_consumer(consumer_id);
    rtmp_pusher.stop();
    encoder.stop();
    consumer_thread.join();
    audio_provider->stop();
    video_provider->stop();
    
    logger->info("rtmp push finished");
    return 0;
}