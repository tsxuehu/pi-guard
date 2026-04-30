#pragma once

#include "audio_frame.hpp"

#include <atomic>
#include <condition_variable>
#include <list>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>

/**
 * @brief 音频捕获提供者
 * 采用单生产者-多消费者模型，参考视频捕获模块实现。
 */
class AudioCaptureProvider {
public:
    using consumer_id_t = uint32_t;

    struct queued_audio {
        std::shared_ptr<audio_frame> frame;
        std::set<consumer_id_t> pending_consumers; // 尚未处理此段的消费者集合
    };

    /**
     * @param device ALSA PCM 名称，如 default、plughw:0,7
     * @param sample_rate_hz 采样率（与 snd_pcm_set_params / 单次 read 的 20ms 片长一致推导）
     * @param channels 通道数（1 为单声道）
     */
    explicit AudioCaptureProvider(std::string device = "default",
                                  unsigned int sample_rate_hz = 16000,
                                  unsigned int channels = 1);

    ~AudioCaptureProvider();

    // 禁用拷贝语义
    AudioCaptureProvider(const AudioCaptureProvider&) = delete;
    AudioCaptureProvider& operator=(const AudioCaptureProvider&) = delete;

    /** 启动 ALSA 采集线程（采样率 / 声道数已由构造传入） */
    bool start();

    /**
     * @brief 停止采集并清理资源
     */
    void stop();

    /**
     * @brief 注册消费者 ID
     */
    consumer_id_t register_consumer();

    /**
     * @brief 注销消费者，并清理其在队列中的状态
     */
    void unregister_consumer(consumer_id_t id);
    
    /**
     * @brief 等待并获取最新可用的音频帧
     * @param id 消费者 ID
     * @param last_seq 上次处理的序号
     * @return 匹配的音频帧指针，若停止则返回 nullptr
     */
    std::shared_ptr<audio_frame> wait_audio(consumer_id_t id, uint64_t last_seq);

private:
    void produce_loop();
    
    // 内部查找逻辑（需在持有锁的情况下调用）
    std::list<queued_audio>::iterator find_latest_frame_locked(consumer_id_t id, uint64_t last_seq);

private:
    std::string device_;
    unsigned int sample_rate_;
    unsigned int channels_;
    const size_t max_queue_capacity_ = 50; // 约 1 秒的缓冲区

    std::atomic<bool> running_{false};
    std::thread produce_thread_;
    
    uint64_t next_seq_ = 0;
    consumer_id_t next_consumer_id_ = 0;
    std::set<consumer_id_t> active_consumers_; 
    
    std::list<queued_audio> queue_;
    std::mutex queue_mtx_;
    std::condition_variable cv_;
};