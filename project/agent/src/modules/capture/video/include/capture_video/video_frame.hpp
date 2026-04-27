#pragma once

#include <memory>
#include <cstdint>
#include <linux/videodev2.h>
#include <sys/ioctl.h>

struct VideoFrame {
    uint64_t seq;           // 全局序列号
    void* data;             // 内存映射地址
    size_t length;          // 帧大小

    // 引用计数核心：利用 shared_ptr 的自定义删除器
    // 当该帧在分发器队列和所有消费者线程中都被释放时，自动执行 QBUF
    std::shared_ptr<void> v4l2_ref;
};
