#pragma once

#include <cstdint>
#include <string>
#include <vector>

class V4L2CaptureSession {
public:
    struct Config {
        std::string device{"/dev/video0"};
        int width{640};
        int height{480};
        uint32_t buffer_count{4};
    };

    explicit V4L2CaptureSession(Config config);
    ~V4L2CaptureSession();

    V4L2CaptureSession(const V4L2CaptureSession&) = delete;
    V4L2CaptureSession& operator=(const V4L2CaptureSession&) = delete;

    int fd() const { return fd_; }
    int width() const { return config_.width; }
    int height() const { return config_.height; }
    const std::vector<void*>& buffer_addrs() const { return buffer_addrs_; }

private:
    struct MMapBuffer {
        void* start{nullptr};
        size_t length{0};
    };

    void configure_format();
    void request_and_map_buffers();
    void queue_all_buffers();
    void stream_on();
    void stream_off() noexcept;
    void unmap_all() noexcept;

    static int xioctl(int fd, unsigned long request, void* arg);

    Config config_;
    int fd_{-1};
    bool stream_on_{false};
    std::vector<MMapBuffer> buffers_;
    std::vector<void*> buffer_addrs_;
};
