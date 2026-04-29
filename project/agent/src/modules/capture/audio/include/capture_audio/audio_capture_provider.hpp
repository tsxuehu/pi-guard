#pragma once

#include <iostream>
#include <vector>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <list>
#include <set>
#include <atomic>
#include <thread>

/**
 * @brief 音频帧数据封装
 */
struct audio_frame {
    uint64_t seq;
    std::vector<int16_t> pcm_data; // PCM S16_LE 格式数据
    uint64_t timestamp;            // 纳秒级或毫秒级时间戳
};

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

    explicit AudioCaptureProvider(const std::string& device = "default");
    ~AudioCaptureProvider();

    // 禁用拷贝语义
    AudioCaptureProvider(const AudioCaptureProvider&) = delete;
    AudioCaptureProvider& operator=(const AudioCaptureProvider&) = delete;

    /**
     * @brief 启动采集线程
     * @param rate 采样率，默认 16000Hz (SpeexDSP 推荐)
     * @param channels 通道数，默认 1 (单声道)
     */
    bool start(unsigned int rate = 16000, unsigned int channels = 1);

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
    unsigned int sample_rate_ = 16000;
    unsigned int channels_ = 1;
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