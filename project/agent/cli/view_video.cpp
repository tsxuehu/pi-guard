#include "capture_video/video_capture_provider.hpp"

#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct MMapBuffer {
    void* start{nullptr};
    size_t length{0};
};

int xioctl(int fd, unsigned long request, void* arg) {
    int ret = -1;
    do {
        ret = ioctl(fd, request, arg);
    } while (ret == -1 && errno == EINTR);
    return ret;
}

uint8_t clamp_to_u8(int value) {
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return static_cast<uint8_t>(value);
}

cv::Mat yuyv_to_bgr(const uint8_t* yuyv, int width, int height) {
    cv::Mat bgr(height, width, CV_8UC3);
    int out_idx = 0;

    for (int i = 0; i < width * height * 2; i += 4) {
        const int y0 = yuyv[i + 0];
        const int u = yuyv[i + 1] - 128;
        const int y1 = yuyv[i + 2];
        const int v = yuyv[i + 3] - 128;

        const int c0 = y0 - 16;
        const int c1 = y1 - 16;

        const int r0 = (298 * c0 + 409 * v + 128) >> 8;
        const int g0 = (298 * c0 - 100 * u - 208 * v + 128) >> 8;
        const int b0 = (298 * c0 + 516 * u + 128) >> 8;

        const int r1 = (298 * c1 + 409 * v + 128) >> 8;
        const int g1 = (298 * c1 - 100 * u - 208 * v + 128) >> 8;
        const int b1 = (298 * c1 + 516 * u + 128) >> 8;

        bgr.data[out_idx + 0] = clamp_to_u8(b0);
        bgr.data[out_idx + 1] = clamp_to_u8(g0);
        bgr.data[out_idx + 2] = clamp_to_u8(r0);
        bgr.data[out_idx + 3] = clamp_to_u8(b1);
        bgr.data[out_idx + 4] = clamp_to_u8(g1);
        bgr.data[out_idx + 5] = clamp_to_u8(r1);
        out_idx += 6;
    }

    return bgr;
}

}  // namespace

int main(int argc, char** argv) {
    const std::string device = (argc > 1) ? argv[1] : "/dev/video0";
    constexpr int kWidth = 640;
    constexpr int kHeight = 480;
    constexpr uint32_t kBufferCount = 4;

    int fd = open(device.c_str(), O_RDWR);
    if (fd < 0) {
        std::cerr << "open camera failed: " << device << ", errno=" << errno << std::endl;
        return 1;
    }

    try {
        v4l2_format fmt{};
        fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        fmt.fmt.pix.width = kWidth;
        fmt.fmt.pix.height = kHeight;
        fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
        fmt.fmt.pix.field = V4L2_FIELD_ANY;
        if (xioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
            throw std::runtime_error("VIDIOC_S_FMT failed");
        }

        v4l2_requestbuffers req{};
        req.count = kBufferCount;
        req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        req.memory = V4L2_MEMORY_MMAP;
        if (xioctl(fd, VIDIOC_REQBUFS, &req) < 0 || req.count < 2) {
            throw std::runtime_error("VIDIOC_REQBUFS failed");
        }

        std::vector<MMapBuffer> buffers(req.count);
        std::vector<void*> addrs(req.count, nullptr);
        for (uint32_t i = 0; i < req.count; ++i) {
            v4l2_buffer buf{};
            buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
            buf.memory = V4L2_MEMORY_MMAP;
            buf.index = i;
            if (xioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
                throw std::runtime_error("VIDIOC_QUERYBUF failed");
            }

            void* start = mmap(nullptr, buf.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, buf.m.offset);
            if (start == MAP_FAILED) {
                throw std::runtime_error("mmap failed");
            }

            buffers[i] = {start, buf.length};
            addrs[i] = start;

            if (xioctl(fd, VIDIOC_QBUF, &buf) < 0) {
                throw std::runtime_error("VIDIOC_QBUF failed");
            }
        }

        v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        if (xioctl(fd, VIDIOC_STREAMON, &type) < 0) {
            throw std::runtime_error("VIDIOC_STREAMON failed");
        }

        auto provider = std::make_shared<video_capture_provider>(fd, 30);
        provider->set_mmap_buffers(std::move(addrs));
        provider->start();
        const auto consumer_id = provider->register_consumer();

        cv::namedWindow("pi-guard-view-video", cv::WINDOW_AUTOSIZE);
        uint64_t last_seq = 0;

        while (true) {
            auto frame = provider->wait_frame(consumer_id, last_seq);
            if (!frame) {
                break;
            }

            if (frame->data != nullptr && frame->length >= static_cast<size_t>(kWidth * kHeight * 2)) {
                const auto* yuyv = static_cast<const uint8_t*>(frame->data);
                cv::Mat image = yuyv_to_bgr(yuyv, kWidth, kHeight);
                cv::imshow("pi-guard-view-video", image);
            }

            last_seq = frame->seq;
            const int key = cv::waitKey(1);
            if (key == 27 || key == 'q' || key == 'Q') {
                break;
            }
        }

        provider->unregister_consumer(consumer_id);
        provider->stop();
        if (xioctl(fd, VIDIOC_STREAMOFF, &type) < 0) {
            std::cerr << "VIDIOC_STREAMOFF failed, errno=" << errno << std::endl;
        }

        for (const auto& buffer : buffers) {
            if (buffer.start != nullptr) {
                munmap(buffer.start, buffer.length);
            }
        }
    } catch (const std::exception& ex) {
        std::cerr << "view_video error: " << ex.what() << std::endl;
        close(fd);
        return 1;
    }

    close(fd);
    return 0;
}
