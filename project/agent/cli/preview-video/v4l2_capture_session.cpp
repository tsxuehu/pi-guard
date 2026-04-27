#include "v4l2_capture_session.hpp"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>

V4L2CaptureSession::V4L2CaptureSession(Config config) : config_(std::move(config)) {
    fd_ = open(config_.device.c_str(), O_RDWR);
    if (fd_ < 0) {
        throw std::runtime_error("open camera failed: " + config_.device + ", errno=" + std::to_string(errno));
    }

    try {
        configure_format();
        request_and_map_buffers();
        queue_all_buffers();
        stream_on();
    } catch (...) {
        unmap_all();
        if (fd_ >= 0) {
            close(fd_);
            fd_ = -1;
        }
        throw;
    }
}

V4L2CaptureSession::~V4L2CaptureSession() {
    stream_off();
    unmap_all();
    if (fd_ >= 0) {
        close(fd_);
    }
}

int V4L2CaptureSession::xioctl(int fd, unsigned long request, void* arg) {
    int ret = -1;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

void V4L2CaptureSession::configure_format() {
    v4l2_format fmt{};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = config_.width;
    fmt.fmt.pix.height = config_.height;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;
    if (xioctl(fd_, VIDIOC_S_FMT, &fmt) < 0) {
        throw std::runtime_error("VIDIOC_S_FMT failed");
    }
}

void V4L2CaptureSession::request_and_map_buffers() {
    v4l2_requestbuffers req{};
    req.count = config_.buffer_count;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    if (xioctl(fd_, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
        throw std::runtime_error("VIDIOC_REQBUFS failed");
    }

    buffers_.resize(req.count);
    buffer_addrs_.resize(req.count, nullptr);
    for (uint32_t i = 0; i < req.count; ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd_, VIDIOC_QUERYBUF, &buf) < 0) {
            throw std::runtime_error("VIDIOC_QUERYBUF failed");
        }

        void* start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, buf.m.offset);
        if (start == MAP_FAILED) {
            throw std::runtime_error("mmap failed");
        }

        buffers_[i] = {start, buf.length};
        buffer_addrs_[i] = start;
    }
}

void V4L2CaptureSession::queue_all_buffers() {
    for (uint32_t i = 0; i < buffers_.size(); ++i) {
        v4l2_buffer buf{};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        if (xioctl(fd_, VIDIOC_QBUF, &buf) < 0) {
            throw std::runtime_error("VIDIOC_QBUF failed");
        }
    }
}

void V4L2CaptureSession::stream_on() {
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (xioctl(fd_, VIDIOC_STREAMON, &type) < 0) {
        throw std::runtime_error("VIDIOC_STREAMON failed");
    }
    stream_on_ = true;
}

void V4L2CaptureSession::stream_off() noexcept {
    if (!stream_on_ || fd_ < 0) {
        return;
    }
    v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    xioctl(fd_, VIDIOC_STREAMOFF, &type);
    stream_on_ = false;
}

void V4L2CaptureSession::unmap_all() noexcept {
    for (const auto& buffer : buffers_) {
        if (buffer.start != nullptr) {
            munmap(buffer.start, buffer.length);
        }
    }
    buffers_.clear();
    buffer_addrs_.clear();
}
