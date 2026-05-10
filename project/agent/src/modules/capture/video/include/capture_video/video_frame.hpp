#pragma once

#include <memory>
#include <cstdint>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

namespace piguard::capture_video {

struct VideoFrame {
    uint64_t seq;           // 全局序列号
    /// steady_clock::now().time_since_epoch().count()，与音频采集一致，便于与音频共时间轴
    uint64_t timestamp_ns{};
    void* data;             // 内存映射地址
    size_t length;          // 帧大小

    // 引用计数核心：利用 shared_ptr 的自定义删除器
    // 当该帧在分发器队列和所有消费者线程中都被释放时，自动执行 QBUF
    std::shared_ptr<void> v4l2_ref;
};

}  // namespace piguard::capture_video
