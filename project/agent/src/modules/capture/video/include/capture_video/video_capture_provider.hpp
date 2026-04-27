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

    void start();
    void stop();
    int capture_fps() const { return capture_fps_; }

    consumer_id_t register_consumer();
    void unregister_consumer(consumer_id_t consumer_id);

    // 供不同速率消费者获取可用帧；自动将更老帧标记为 skipped
    std::shared_ptr<VideoFrame> wait_frame(consumer_id_t consumer_id, uint64_t last_seq);

private:
    struct queued_frame {
        std::shared_ptr<VideoFrame> frame;
        std::unordered_set<consumer_id_t> pending_consumers;
    };

    struct mmap_buffer {
        void* start{nullptr};
        size_t length{0};
    };

    bool init_v4l2_capture();
    bool configure_v4l2_capture();
    bool request_and_map_buffers();
    bool queue_all_buffers();
    bool stream_on();
    void stream_off() noexcept;
    void unmap_all() noexcept;
    void close_fd() noexcept;
    void produce_loop();
    void remove_consumer_from_pending_locked(consumer_id_t consumer_id);
    void prune_finished_frames_locked();

    std::string device_;
    int fd_;
    int capture_fps_;
    int capture_width_;
    int capture_height_;
    size_t max_capacity_;
    uint64_t global_seq_{0};
    consumer_id_t next_consumer_id_{1};
    std::atomic<bool> running_{false};
    bool stream_on_{false};
    std::vector<mmap_buffer> mmap_buffers_;
    std::unordered_set<consumer_id_t> consumers_;
    std::deque<queued_frame> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread cap_thread_;
};
