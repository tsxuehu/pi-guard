#pragma once

#include "video_frame.hpp"
#include <cstdint>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <thread>
#include <unordered_set>
#include <vector>

class video_capture_provider {
public:
    using consumer_id_t = uint64_t;

    explicit video_capture_provider(int v4l2_fd, size_t max_capacity = 10);
    ~video_capture_provider();

    // 禁用拷贝
    video_capture_provider(const video_capture_provider&) = delete;
    video_capture_provider& operator=(const video_capture_provider&) = delete;

    void start();
    void stop();
    void set_mmap_buffers(std::vector<void*> buffer_addrs);

    consumer_id_t register_consumer();
    void unregister_consumer(consumer_id_t consumer_id);

    // 供不同速率消费者获取可用帧；自动将更老帧标记为 skipped
    std::shared_ptr<video_frame> wait_frame(consumer_id_t consumer_id, uint64_t last_seq);

private:
    struct queued_frame {
        std::shared_ptr<video_frame> frame;
        std::unordered_set<consumer_id_t> pending_consumers;
    };

    void produce_loop();
    void remove_consumer_from_pending_locked(consumer_id_t consumer_id);
    void prune_finished_frames_locked();

    int fd_;
    size_t max_capacity_;
    uint64_t global_seq_{0};
    consumer_id_t next_consumer_id_{1};
    std::atomic<bool> running_{false};
    std::vector<void*> mmap_buffers_;
    std::unordered_set<consumer_id_t> consumers_;
    std::deque<queued_frame> queue_;
    std::mutex mtx_;
    std::condition_variable cv_;
    std::thread cap_thread_;
};
