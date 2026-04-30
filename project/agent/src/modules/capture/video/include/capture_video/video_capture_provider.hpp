#pragma once

#include "video_frame.hpp"
#include <cstdint>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <string>
#include <unordered_set>
#include <vector>

namespace piguard::capture_video {

class VideoCaptureProvider {
public:
    using consumer_id_t = uint64_t;

    VideoCaptureProvider(
        std::string device,
        int capture_fps,
        int capture_width,
        int capture_height,
        size_t max_capacity = 10);
    ~VideoCaptureProvider();

    // 禁用拷贝
    VideoCaptureProvider(const VideoCaptureProvider&) = delete;
    VideoCaptureProvider& operator=(const VideoCaptureProvider&) = delete;

    // 启动采集线程并初始化 v4l2 资源；重复调用无副作用。
    void start();
    // 停止采集线程并释放 v4l2 资源（stream off / unmap / close）。
    void stop();
    // 返回当前配置的源采集帧率（fps）。
    int capture_fps() const { return capture_fps_; }

    // 注册一个消费者并返回其唯一 ID，用于后续拉帧与反注册。
    consumer_id_t register_consumer();
    // 注销指定消费者，并清理其在队列中的 pending 标记。
    void unregister_consumer(consumer_id_t consumer_id);

    // 供不同速率消费者获取可用帧；返回所有 seq > last_seq 的可用帧
    std::vector<std::shared_ptr<VideoFrame>> wait_frame(consumer_id_t consumer_id, uint64_t last_seq);

private:
    struct queued_frame {
        std::shared_ptr<VideoFrame> frame;
        std::unordered_set<consumer_id_t> pending_consumers;
    };

    struct mmap_buffer {
        void* start{nullptr};
        size_t length{0};
    };

    // 执行 v4l2 初始化总流程：open -> 配置 -> reqbufs+mmap -> qbuf -> stream on。
    bool init_v4l2_capture();
    // 配置采集参数（分辨率/像素格式/帧率）。
    bool configure_v4l2_capture();
    // 申请并映射内核缓冲区到用户态地址空间。
    bool request_and_map_buffers();
    // 将所有 mmap 缓冲区入队，供驱动填充采集数据。
    bool queue_all_buffers();
    // 开启视频流采集（VIDIOC_STREAMON）。
    bool stream_on();
    // 关闭视频流采集（VIDIOC_STREAMOFF）；失败时仅吞错，确保析构安全。
    void stream_off() noexcept;
    // 解除所有 mmap 映射并清空缓冲区元信息。
    void unmap_all() noexcept;
    // 关闭设备 fd，并将其重置为无效值。
    void close_fd() noexcept;
    // 采集线程主循环：DQBUF -> 生成帧 -> 入队分发 -> 引用释放时 QBUF。
    void produce_loop();
    // 要求调用方已持锁；将消费者从所有帧的 pending 集合移除。
    void remove_consumer_from_pending_locked(consumer_id_t consumer_id);
    // 要求调用方已持锁；按模式移除消费者 pending 并回收已完成帧。
    void cleanup_consumer_pending_locked(consumer_id_t consumer_id, uint64_t last_seq, bool clear_all);

    std::string device_;                   // 采集设备路径，例如 /dev/video0
    int fd_;                               // 设备文件描述符；打开后用于所有 v4l2 ioctl 调用
    int capture_fps_;                      // 期望采集帧率（用于 VIDIOC_S_PARM）
    int capture_width_;                    // 期望采集宽度（用于 VIDIOC_S_FMT）
    int capture_height_;                   // 期望采集高度（用于 VIDIOC_S_FMT）
    size_t max_capacity_;                  // 分发队列最大帧数，超出时丢弃最旧帧
    uint64_t global_seq_{0};               // 全局递增帧序号，标识帧先后顺序
    consumer_id_t next_consumer_id_{1};    // 下一个消费者 ID 生成器
    std::atomic<bool> running_{false};     // 采集线程运行标志（start/stop 共享）
    bool stream_on_{false};                // v4l2 stream 是否已开启，避免重复 STREAMOFF
    std::vector<mmap_buffer> mmap_buffers_;// mmap 缓冲区信息；按 v4l2 buffer index 索引
    std::unordered_set<consumer_id_t> consumers_; // 当前已注册消费者集合
    std::deque<queued_frame> queue_;       // 帧分发队列；每帧记录待消费的消费者集合
    std::mutex mtx_;                       // 保护消费者集合与队列等共享状态
    std::condition_variable queue_cv_;     // 帧到达/停止时唤醒等待中的消费者
    std::thread cap_thread_;               // 后台采集线程（DQBUF -> 入队 -> 通知）
};

}  // namespace piguard::capture_video
