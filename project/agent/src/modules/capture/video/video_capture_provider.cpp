#include "capture_video/video_capture_provider.hpp"

#include <cerrno>
#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <utility>

namespace {
constexpr uint32_t kDefaultBufferCount = 4;

int xioctl(int fd, unsigned long request, void* arg) {
    int ret = -1;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}
}  // namespace

namespace piguard::capture_video {

VideoCaptureProvider::VideoCaptureProvider(
    std::string device, int capture_fps, int capture_width, int capture_height, size_t max_capacity)
    : device_(std::move(device)),
      fd_(-1),
      capture_fps_(capture_fps),
      capture_width_(capture_width),
      capture_height_(capture_height),
      max_capacity_(max_capacity) {}

VideoCaptureProvider::~VideoCaptureProvider() {
    stop();
}

void VideoCaptureProvider::start() {
    if (running_.exchange(true)) return;
    cap_thread_ = std::thread(&VideoCaptureProvider::produce_loop, this);
}

void VideoCaptureProvider::stop() {
    running_ = false;
    cv_.notify_all();
    stream_off();
    if (cap_thread_.joinable()) cap_thread_.join();
    unmap_all();
    close_fd();
}

bool VideoCaptureProvider::init_v4l2_capture() {
    // 重新初始化前先确保旧 fd 已关闭，避免重复打开泄漏。
    close_fd();
    fd_ = open(device_.c_str(), O_RDWR);
    if (fd_ < 0) {
        return false;
    }
    if (!configure_v4l2_capture() || !request_and_map_buffers() || !queue_all_buffers() || !stream_on()) {
        stream_off();
        unmap_all();
        close_fd();
        return false;
    }
    return true;
}

VideoCaptureProvider::consumer_id_t VideoCaptureProvider::register_consumer() {
    std::lock_guard lock(mtx_);
    const consumer_id_t id = next_consumer_id_++;
    consumers_.insert(id);
    return id;
}

void VideoCaptureProvider::remove_consumer_from_pending_locked(consumer_id_t consumer_id) {
    for (auto& item : queue_) {
        item.pending_consumers.erase(consumer_id);
    }
}

void VideoCaptureProvider::cleanup_consumer_pending_locked(
    consumer_id_t consumer_id, uint64_t last_seq, bool clear_all) {
    for (auto it = queue_.begin(); it != queue_.end(); ) {
        const bool should_clear =
            clear_all || (it->frame->seq > last_seq && it->pending_consumers.count(consumer_id) > 0);
        if (should_clear) {
            it->pending_consumers.erase(consumer_id);
        }
        if (it->pending_consumers.empty()) {
            it = queue_.erase(it);
        } else {
            ++it;
        }
    }
}

void VideoCaptureProvider::unregister_consumer(consumer_id_t consumer_id) {
    std::lock_guard lock(mtx_);
    consumers_.erase(consumer_id);
    cleanup_consumer_pending_locked(consumer_id, 0, true);
    cv_.notify_all();
}

bool VideoCaptureProvider::configure_v4l2_capture() {
    if (fd_ < 0 || capture_fps_ <= 0 || capture_width_ <= 0 || capture_height_ <= 0) {
        return false;
    }

    // 配置分辨率与像素格式。
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = static_cast<uint32_t>(capture_width_);
    fmt.fmt.pix.height = static_cast<uint32_t>(capture_height_);
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        return false;
    }

    // 配置采集帧率（timeperframe = 1 / capture_fps_）。
    v4l2_streamparm parm{};
    parm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    parm.parm.capture.timeperframe.numerator = 1;
    parm.parm.capture.timeperframe.denominator = static_cast<uint32_t>(capture_fps_);
    if (xioctl(fd_, VIDIOC_S_PARM, &parm) < 0) {
        return false;
    }
    return true;
}

bool VideoCaptureProvider::request_and_map_buffers() {
    // 申请驱动缓冲区并映射到用户态，后续通过 buf.index 定位地址。
    v4l2_requestbuffers req{};
    req.count = kDefaultBufferCount;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        return false;
    }

    mmap_buffers_.resize(req.count);
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            return false;
        }

        void* start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
        if (start == MAP_FAILED) {
            return false;
        }
        mmap_buffers_[i] = {start, buf.length};
    }
    return true;
}

bool VideoCaptureProvider::queue_all_buffers() {
    // 所有缓冲区先入队，驱动才能开始填帧。
    for (uint32_t i = 0; i < mmap_buffers_.size(); ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            return false;
        }
    }
    return true;
}

bool VideoCaptureProvider::stream_on() {
    if (fd_ < 0) {
        return false;
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        return false;
    }
    stream_on_ = true;
    return true;
}

void VideoCaptureProvider::stream_off() noexcept {
    if (!stream_on_ || fd_ < 0) {
        return;
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    (void)xioctl(fd_, VIDIOC_STREAMOFF, &type);
    stream_on_ = false;
}

void VideoCaptureProvider::unmap_all() noexcept {
    // 释放所有映射，避免进程退出前遗留 mmap 区域。
    for (const auto& buffer : mmap_buffers_) {
        if (buffer.start != nullptr) {
            (void)munmap(buffer.start, buffer.length);
        }
    }
    mmap_buffers_.clear();
}

void VideoCaptureProvider::close_fd() noexcept {
    if (fd_ >= 0) {
        close(fd_);
        fd_ = -1;
    }
}

void VideoCaptureProvider::produce_loop() {
    if (!init_v4l2_capture()) {
        running_ = false;
        cv_.notify_all();
        return;
    }

    while (running_) {
        // 从驱动出队一帧；失败时让出时间片并继续重试。
        struct v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;

        if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
            std::this_thread::yield();
            continue;
        }

        auto frame = std::make_shared<VideoFrame>();
        frame->seq = ++global_seq_;
        frame->data = (buf.index < mmap_buffers_.size()) ? mmap_buffers_[buf.index].start : nullptr;
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
        }
        cv_.notify_all();
    }
}

std::vector<std::shared_ptr<VideoFrame>> VideoCaptureProvider::wait_frame(
    consumer_id_t consumer_id, uint64_t last_seq) {
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
        return {};
    }

    std::vector<std::shared_ptr<VideoFrame>> matched_frames;
    for (size_t i = 0; i < queue_.size(); ++i) {
        const auto& item = queue_[i];
        if (item.frame->seq > last_seq && item.pending_consumers.count(consumer_id) > 0) {
            matched_frames.push_back(item.frame);
        }
    }

    if (matched_frames.empty()) {
        return {};
    }

    // 将命中的帧标记为该消费者已处理。
    cleanup_consumer_pending_locked(consumer_id, last_seq, false);
    return matched_frames;
}

}  // namespace piguard::capture_video
