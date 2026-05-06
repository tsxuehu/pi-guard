# Encoder 模块技术方案

## 概述

Encoder 是基于 FFmpeg 的音视频实时编码器模块，将 V4L2 摄像头/ALSA 麦克风采集的原始数据编码为 H.264 和 AAC 格式，通过多消费者队列分发给下游（文件录制、RTMP 推流等）。核心设计关注点：低延迟、多消费者独立进度、音视频独立线程。

位于 `src/modules/processing/encoder/`。

## 模块架构

```
┌──────────────────────────┐
│     IVideoFrameGetter    │  ← 接口：从外部取视频帧
│   (fetch_frames)          │
└──────────┬───────────────┘
           │ VideoProviderAdapter
           ▼
┌──────────────────────────┐
│     IAudioFrameGetter    │  ← 接口：从外部取音频帧
│   (fetch_frames)          │
└──────────┬───────────────┘
           │ AudioProviderAdapter
           ▼
┌─────────────────────────────────────────────────────┐
│                    Encoder                           │
│                                                     │
│  ┌─────────────────────┐  ┌─────────────────────┐  │
│  │ video_encode_loop() │  │ audio_encode_loop() │  │
│  │     (独立线程)        │  │     (独立线程)        │  │
│  │                     │  │                     │  │
│  │ YUYV422 → YUV420P  │  │ S16 → FLTP          │  │
│  │   (sws_scale)       │  │   (swr_convert)      │  │
│  │        ↓            │  │        ↓            │  │
│  │   H.264 编码        │  │   AAC 编码           │  │
│  │ avcodec_send/receive│  │ avcodec_send/receive │  │
│  └─────────┬───────────┘  └─────────┬───────────┘  │
│            │                        │               │
│            └────────┬───────────────┘               │
│                     ▼                               │
│         ┌───────────────────────┐                   │
│         │    packet_queue_      │   多消费者队列     │
│         │  (deque<QueuedPacket>)│                   │
│         └───────────┬───────────┘                   │
└─────────────────────┼───────────────────────────────┘
                      │ wait_packet
          ┌───────────┴───────────┐
          ▼                       ▼
     ┌──────────┐           ┌──────────┐
     │ 消费者 A  │           │ 消费者 B  │
     │(mp4写文件)│           │(RTMP推流) │
     └──────────┘           └──────────┘
```

## 类型体系

### 编码结果类型（继承多态）

```cpp
EncodedPacketBase                  ← 基类，虚析构支持 dynamic_cast
├── seq, data, pts, dts           ← 共有字段
│
├── EncodedVideoPacket            ← 视频子类
│   └── key_frame                 ← 仅视频有关键帧概念
│
└── EncodedAudioPacket            ← 音频子类（AAC 每帧均关键帧，无需 key_frame）
```

### 流元数据类型（继承多态）

```cpp
EncodedStreamMetaBase              ← 基类
├── ready, codec_id               ← 共有字段
├── time_base_num, time_base_den
├── extradata                     ← sps/pps 等编码器配置
│
├── EncodedVideoStreamMeta        ← 视频子类
│   └── width, height
│
└── EncodedAudioStreamMeta        ← 音频子类
    └── sample_rate, channels
```

消费端使用 `auto` 推导 `video_stream_meta()` / `audio_stream_meta()` 的返回值，天然落在正确的子类类型上，使用时无需手动转换。

### PIMPL 封装 FFmpeg 依赖

```cpp
// encoder.hpp（头文件，不暴露 FFmpeg 类型）
struct VideoCodecContext;            // 前向声明，不完整类型
struct AudioCodecContext;
std::unique_ptr<VideoCodecContext> video_ctx_;  // unique_ptr 可持有不完整类型
std::unique_ptr<AudioCodecContext> audio_ctx_;

// encoder.cpp（实现，完整定义锁在这里）
struct Encoder::VideoCodecContext {
    AVCodecContext* codec_ctx{nullptr};
    AVFrame* frame{nullptr};
    AVPacket* packet{nullptr};
    SwsContext* sws_ctx{nullptr};
    int64_t pts{0};
};
```

所有 FFmpeg 类型依赖被锁在 `.cpp` 中，修改 FFmpeg 版本或参数不会导致外部模块重编译。

## 运行流程

### 1. 构造

```cpp
Encoder(video_getter, audio_getter, options)
```

接收两个 getter 接口（可为 `nullptr` 表示纯音频/纯视频场景）和编码参数。此时不分配任何 FFmpeg 资源。

### 2. start() — 初始化并启动线程

```
start()
  ├── running_.exchange(true) 防重入
  ├── audio_pcm_buf_.clear()  清空残留缓冲
  ├── init_video_encoder()    初始化 H.264 编码器
  │     ├── avcodec_find_encoder(H264)
  │     ├── 设置 width/height/time_base/bitrate/gop
  │     ├── preset=veryfast, tune=zerolatency
  │     ├── avcodec_open2()
  │     ├── av_frame_alloc() + av_packet_alloc()
  │     ├── sws_getContext(YUYV422 → YUV420P)
  │     └── 填充 video_meta_ 供消费者读取
  ├── init_audio_encoder()    初始化 AAC 编码器
  │     ├── avcodec_find_encoder(AAC)
  │     ├── 设置 sample_rate/bitrate/ch_layout/time_base
  │     ├── avcodec_open2()
  │     ├── swr_alloc_set_opts2(S16 → FLTP)  ← 仅在编码器不是 S16 时创建
  │     └── 填充 audio_meta_
  ├── 启动 video_thread_（如果 video_getter_ 非空）
  └── 启动 audio_thread_（如果 audio_getter_ 非空）
```

初始化过程中任一环节失败都有 error 日志定位具体原因；成功后输出 debug 日志包含编码参数。

### 3. 视频编码循环 video_encode_loop()

```
while (running_) {
    frames = video_getter_->fetch_frames()  ← 返回 vector，可能多帧
    if 空或无 ctx → continue

    // 实时丢帧策略：积压多帧时只取最新
    if (frames.size() > 1) → log 丢弃 N-1 帧
    video_frame = frames.back()

    if 帧数据无效 → continue

    av_frame_make_writable()
    sws_scale(YUYV422 → YUV420P)          ← 格式转换
    frame->pts = pts++
    avcodec_send_frame()                  ← 送入编码器

    // 异步模型：一次 send 可能产生 0~N 个包
    while (avcodec_receive_packet() == 0) {
        构造 EncodedVideoPacket
        enqueue_packet()
    }
}
```

### 4. 音频编码循环 audio_encode_loop()

```
while (running_) {
    frames = audio_getter_->fetch_frames()
    if 空或无 ctx → continue

    // 多帧 PCM 数据全部追加到缓冲
    for (frame : frames) {
        audio_pcm_buf_.insert(frame->pcm_data)
    }

    // 缓冲够一帧 AAC 才编码
    while (audio_pcm_buf_.size() >= 1024 * channels) {
        从缓冲头部取 1024 样本
        swr_convert(S16 → FLTP) 或 memcpy(S16→S16)
        frame->pts = pts; pts += 1024
        avcodec_send_frame()

        while (avcodec_receive_packet() == 0) {
            构造 EncodedAudioPacket
            enqueue_packet()
        }

        audio_pcm_buf_.erase(已编码部分)  ← 保留余数到下次
    }
}
```

### 5. 多消费者队列

```
enqueue_packet(packet)
  ├── seq = ++packet_seq_                    全局递增序列号
  ├── packet_queue_.push_back({packet, consumers_})  关联所有当前消费者
  ├── 超出 capacity → pop_front + log 溢出
  └── notify_all 唤醒等待消费者

wait_packet(consumer_id, last_seq)
  ├── 阻塞等待：有新包且该消费者未取过
  ├── 收集所有 seq > last_seq 且 pending 中有该消费者的包
  ├── cleanup: 从这些包的 pending_consumers 中移除该消费者
  │   （当所有消费者都取走后，包从队列中释放）
  └── return packets
```

每个消费者只需维护自己的 `last_seq`，即可独立获取增量数据。消费者掉线后自动清理引用，包在所有消费者取走后释放。

### 6. stop() — 优雅退出

```
stop()
  ├── running_.exchange(false)       通知编码线程退出
  ├── packet_cv_.notify_all()        唤醒阻塞在 wait_packet 中的消费者
  ├── video_thread_.join()           等待视频线程结束
  ├── audio_thread_.join()           等待音频线程结束
  ├── flush_video_encoder()          刷出编码器内部残留帧（send nullptr）
  ├── flush_audio_encoder()          刷出编码器内部残留帧
  ├── close_video_encoder()          释放 FFmpeg 资源
  └── close_audio_encoder()
```

Flush 机制：`avcodec_send_frame(ctx, nullptr)` 告知编码器无更多输入，然后循环 `avcodec_receive_packet()` 取尽缓冲区中的最后帧，确保 GOP 尾部不丢帧。

## 关键技术点

### 低延迟配置

| 参数 | 取值 | 作用 |
|---|---|---|
| `preset` | `veryfast` | 减少编码计算量，降低端到端延迟 |
| `tune` | `zerolatency` | 关闭编码器内部帧缓冲，不等待未来帧做参考 |
| `max_b_frames` | 0 | 不使用 B 帧，避免需要等待后续帧才能解码 |
| `gop_size` | fps | 每 1 秒一个关键帧，兼顾延迟和压缩率 |

### 音频 PCM 缓冲

AAC 编码器标准帧为 1024 个样本，但 ALSA 每次只采集约 320 个样本（16000Hz × 20ms）。`audio_pcm_buf_` 作为缓冲区将多次小片 PCM 累积，够一帧才送入编码器。

```
ALSA 采集 320 → 缓冲 320
ALSA 采集 320 → 缓冲 640
ALSA 采集 320 → 缓冲 960
ALSA 采集 320 → 缓冲 1280 ≥ 1024 → 编码 1024，余 256
```

### 实时丢帧策略

视频编码线程采用"保留最新帧"策略：`fetch_frames()` 返回多帧时，只取 `frames.back()` 编码，其余丢弃。视频与音频不同 — 积压的视频帧是"过去"的画面，编码出来也无意义。

### send/receive 异步模型

FFmpeg 编码是异步的：`send_frame(1帧)` 与 `receive_packet()` 不是一一对应。

| 场景 | send 后 receive 次数 |
|---|---|
| 普通帧 | 1 次 |
| 编码器内部缓冲 | 0 次 |
| GOP 边界 | 2+ 次（一次吐出多个残留包） |
| flush (send nullptr) | 多次（吐尽缓冲） |

因此编码循环中始终使用 `while (receive_packet() == 0)` 而非 `if`，确保不丢包。

### 日志体系

| 级别 | 触发条件 |
|---|---|
| **info** | 启动成功、停止完成 |
| **error** | 编码器未找到、分配/打开失败、帧缓冲失败、格式转换器失败 |
| **debug** | 初始化成功（含参数）、视频丢帧、音视频首帧编码成功 |
| **warn** | 不支持的音频采样格式、编码发送失败、包队列溢出丢包 |

### 多消费者独立进度

每个消费者注册时获得唯一 ID，入队时包关联所有活跃消费者。消费者通过 `wait_packet(id, last_seq)` 按自己的进度拉取增量，互不干扰。消费者注销时自动清除其引用，当所有消费者取走后包被释放，避免内存泄漏。
