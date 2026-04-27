#include "capture_video/video_capture_provider.hpp"
#include <utility>

video_capture_provider::video_capture_provider(int v4l2_fd, size_t max_capacity)
    : fd_(v4l2_fd), max_capacity_(max_capacity) {}

video_capture_provider::~video_capture_provider() {
    stop();
}

void video_capture_provider::start() {
    if (running_.exchange(true)) return;
    cap_thread_ = std::thread(&video_capture_provider::produce_loop, this);
}

void video_capture_provider::stop() {
    running_ = false;
    cv_.notify_all();
    if (cap_thread_.joinable()) cap_thread_.join();
}

void video_capture_provider::set_mmap_buffers(std::vector<void*> buffer_addrs) {
    std::lock_guard lock(mtx_);
    mmap_buffers_ = std::move(buffer_addrs);
}

video_capture_provider::consumer_id_t video_capture_provider::register_consumer() {
    std::lock_guard lock(mtx_);
    const consumer_id_t id = next_consumer_id_++;
    consumers_.insert(id);
    return id;
}

void video_capture_provider::remove_consumer_from_pending_locked(consumer_id_t consumer_id) {
    for (auto& item : queue_) {
        item.pending_consumers.erase(consumer_id);
    }
}

void video_capture_provider::prune_finished_frames_locked() {
    while (!queue_.empty() && queue_.front().pending_consumers.empty()) {
        queue_.pop_front();
    }
}

void video_capture_provider::unregister_consumer(consumer_id_t consumer_id) {
    std::lock_guard lock(mtx_);
    consumers_.erase(consumer_id);
    remove_consumer_from_pending_locked(consumer_id);
    prune_finished_frames_locked();
    cv_.notify_all();
}

void video_capture_provider::produce_loop() {
    while (running_) {
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
            std::this_thread::yield();
            continue;
        }

        auto frame = std::make_shared<video_frame>();
        frame->seq = ++global_seq_;
        frame->data = (buf.index < mmap_buffers_.size()) ? mmap_buffers_[buf.index] : nullptr;
        frame->length = buf.bytesused;

        // 【关键】引用计数回收逻辑
        // 只有当队列移除该帧，且所有消费者都处理完该帧时，Lambda 才会执行。
        frame->v4l2_ref = std::shared_ptr<void>(nullptr, [fd = this->fd_, idx = buf.index](void*) {
            struct v4l2_buffer b{};
            b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            b.memory = V4L2_MEMORY_MMAP;
            b.index = idx;
            ioctl(fd, VIDIOC_QBUF, &b);
        });

        {
            std::lock_guard lock(mtx_); // C++17 CTAD
            queued_frame item;
            item.frame = std::move(frame);
            item.pending_consumers = consumers_;
            queue_.push_back(std::move(item));

            // 满容量时直接丢最老帧。
            while (queue_.size() > max_capacity_) {
                queue_.pop_front();
            }

            prune_finished_frames_locked();
        }
        cv_.notify_all();
    }
}

std::shared_ptr<video_frame> video_capture_provider::wait_frame(consumer_id_t consumer_id, uint64_t last_seq) {
    std::unique_lock lock(mtx_);
    cv_.wait(lock, [this, consumer_id, last_seq] {
        if (!running_) {
            return true;
        }
        for (const auto& item : queue_) {
            if (item.frame->seq > last_seq && item.pending_consumers.count(consumer_id) > 0) {
                return true;
            }
        }
        return false;
    });

    if (!running_ || queue_.empty() || consumers_.count(consumer_id) == 0) {
        return nullptr;
    }

    size_t selected_idx = queue_.size();
    for (size_t i = 0; i < queue_.size(); ++i) {
        const auto& item = queue_[i];
        if (item.frame->seq > last_seq && item.pending_consumers.count(consumer_id) > 0) {
            selected_idx = i;
        }
    }

    if (selected_idx == queue_.size()) {
        return nullptr;
    }

    // 慢消费者追到更新帧时，旧帧按 skipped 处理。
    for (size_t i = 0; i <= selected_idx; ++i) {
        queue_[i].pending_consumers.erase(consumer_id);
    }

    auto out = queue_[selected_idx].frame;
    prune_finished_frames_locked();
    return out;
}
