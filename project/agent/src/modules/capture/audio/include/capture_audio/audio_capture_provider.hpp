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
#include <vector>

namespace piguard::capture_audio {

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
     * 须显式传入三项参数（无默认值）。非法参数抛出 std::invalid_argument。
     *
     * @param device ALSA PCM 名称，如 default、plughw:0,7
     * @param sample_rate_hz 采样率（与 snd_pcm_set_params / 单次 read 的 ~20ms 片长推导）
     * @param channels 声道数（须 >= 1）
     */
    explicit AudioCaptureProvider(std::string device, unsigned int sample_rate_hz, unsigned int channels);

    ~AudioCaptureProvider();

    // 禁用拷贝语义
    AudioCaptureProvider(const AudioCaptureProvider&) = delete;
    AudioCaptureProvider& operator=(const AudioCaptureProvider&) = delete;

    /** 启动 ALSA 采集线程 */
    bool start();

    /**
     * 停止采集并清理资源
     */
    void stop();

    /**
     * 注册消费者 ID
     */
    consumer_id_t register_consumer();

    /**
     * 注销消费者，并清理其在队列中的状态
     */
    void unregister_consumer(consumer_id_t id);
    
    /**
     * @brief 等待并获取所有满足条件的音频帧
     * @param id 消费者 ID
     * @param last_seq 上次处理的序号
     * @return 匹配的音频帧列表，若停止或无可用帧则返回空列表
     */
    std::vector<std::shared_ptr<audio_frame>> wait_audio(consumer_id_t id, uint64_t last_seq);

private:
    void produce_loop();
    // 需在持有 queue_mtx_ 时调用；按模式清理消费者 pending 并回收空节点。
    void cleanup_consumer_pending_locked(consumer_id_t id, uint64_t last_seq, bool clear_all);

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
    std::condition_variable queue_cv_;

};

}  // namespace piguard::capture_audio