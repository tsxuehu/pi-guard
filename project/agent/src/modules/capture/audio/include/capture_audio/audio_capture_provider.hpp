#pragma once

#include "audio_frame.hpp"

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
        std::shared_ptr<AudioFrame> frame;
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

    /** 启动 ALSA 采集线程；初始化失败抛 std::runtime_error。 */
    void start();

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
    std::vector<std::shared_ptr<AudioFrame>> wait_audio(consumer_id_t id, uint64_t last_seq);

private:
    void produce_loop();
    void report_start_result(bool ok);
    // 在 state_mtx_ 下置 running_=false 并唤醒等待者；返回此前是否处于运行态。
    bool mark_stopped();
    // 需在持有 state_mtx_ 时调用；按模式清理消费者 pending 并回收空节点。
    void cleanup_consumer_pending_locked(consumer_id_t id, uint64_t last_seq, bool clear_all);

private:
    std::string device_;
    unsigned int sample_rate_;
    unsigned int channels_;
    const size_t max_queue_capacity_ = 50; // 约 1 秒的缓冲区

    // state_mtx_ 保护下方所有共享状态：running_/queue_/active_consumers_/next_seq_ 等。
    // state_cv_ 在「有新帧可取」或「停止」时被通知，谓词读到的状态与本锁一致。
    std::mutex state_mtx_;
    std::condition_variable state_cv_;

    bool running_{false};
    std::thread produce_thread_;

    std::mutex start_mtx_;
    std::condition_variable start_cv_;
    bool start_reported_{false};
    bool start_ok_{false};

    uint64_t next_seq_ = 0;
    consumer_id_t next_consumer_id_ = 0;
    std::set<consumer_id_t> active_consumers_;

    std::list<queued_audio> queue_;

};

}  // namespace piguard::capture_audio