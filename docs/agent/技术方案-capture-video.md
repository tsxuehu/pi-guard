# 视频捕获模块技术说明

## 1. 模块功能

视频捕获模块用于通过 V4L2 从摄像头持续采集视频帧，并将帧分发给多个不同速率的消费者线程。模块目标如下：

- 支持单生产者、多消费者并发访问。
- 支持消费者以不同帧率消费（例如 20fps、30fps）。
- 支持同一帧被多个消费者读取。
- 队列有容量上限，超限时丢弃最老帧，保证系统低延迟。
- 当某帧被所有消费者“消费或跳过”后，自动清理并触发底层 buffer 回收。

## 2. 对外接口

当前主要由 `video_capture_provider` 提供对外能力：

- `start()`  
  启动采集线程，进入 `VIDIOC_DQBUF` 循环拉帧。

- `stop()`  
  停止采集线程，唤醒所有等待中的消费者并退出。

- `consumer_id_t register_consumer()`  
  注册一个消费者，返回消费者 ID。后续按该 ID 跟踪帧消费状态。

- `void unregister_consumer(consumer_id_t consumer_id)`  
  注销消费者。注销后该消费者对所有待处理帧视为不再参与，相关帧状态会立即更新。

- `std::shared_ptr<video_frame> wait_frame(consumer_id_t consumer_id, uint64_t last_seq)`  
  等待并获取该消费者可用的新帧。内部会自动将该消费者在更旧帧上的状态标记为“跳过”。

消费者基类 `consumer_base` 的使用方式：

- 构造时自动注册消费者；
- `run()` 循环调用 `wait_frame()` 获取帧并处理；
- 析构时自动注销消费者，避免悬挂状态。

## 3. V4L2 设备操作流程

底层实现在 `VideoCaptureProvider` 中完成；业务侧一般只需 `start()` 启动、`register_consumer()` + `wait_frame()` 消费、`stop()` 收尾。与代码一致的设备侧步骤如下。

### 3.1 打开设备

- 采集线程启动后进入 `produce_loop()`，首先调用 `init_v4l2_capture()`。
- 若之前有残留句柄，先 `close_fd()`，再用 `open(device_.c_str(), O_RDWR)` 打开设备节点（通常为 `/dev/video0`）。
- 打开失败则 `running_ = false`，并唤醒等待中的消费者后立即返回。

### 3.2 配置设备

`init_v4l2_capture()` 在打开 fd 成功后，按顺序完成配置与就绪，任一步失败会 `STREAMOFF`、`unmap_all`、`close_fd()` 并返回 `false`。

1. **`configure_v4l2_capture()`**（`VIDIOC_S_FMT`、`VIDIOC_S_PARM`）  
   - 校验：`fd` 有效，且 `capture_fps_`、`capture_width_`、`capture_height_` 均大于 0。  
   - 格式：`V4L2_BUF_TYPE_VIDEO_CAPTURE`，像素格式固定为 `V4L2_PIX_FMT_YUYV`，分辨率取自构造参数，`field` 为 `V4L2_FIELD_ANY`。  
   - 帧率：通过 `timeperframe` 配置为 \(1 / \mathrm{capture\_fps\_}\) 秒（分子 1，分母为 `capture_fps_`）。

2. **`request_and_map_buffers()`**  
   - `VIDIOC_REQBUFS`：申请 `count = 4` 块、`V4L2_MEMORY_MMAP` 的捕获缓冲区（至少成功分配 2 块）。  
   - 对每个 index：`VIDIOC_QUERYBUF` 后 `mmap`，得到用户态可读写的 `mmap_buffers_[index]`。

3. **`queue_all_buffers()`**  
   - 对每个缓冲区 `VIDIOC_QBUF`，使驱动可往缓冲区填帧。

4. **`stream_on()`**  
   - `VIDIOC_STREAMON` 开启 `V4L2_BUF_TYPE_VIDEO_CAPTURE` 捕获流。

### 3.3 获取视频帧

- **生产者（采集线程）**  
  在 `running_` 为真时循环：`VIDIOC_DQBUF` 从驱动取出一帧；按 `buf.index` 绑定 `mmap_buffers_` 中地址与 `buf.bytesused` 填入 `VideoFrame`（含递增 `seq`）；用带自定义删除器的 `v4l2_ref` 在引用归零时对同一 index 执行 `VIDIOC_QBUF`，将缓冲区归还驱动；帧再放入内部分发队列并 `notify_all` 消费者。  
  若 `DQBUF` 失败则 `yield()` 后继续重试，避免忙等。

- **消费者**  
  先 `register_consumer()` 取得 ID；在自有循环里调用 `wait_frame(consumer_id, last_seq)`，在队列中选取对该消费者可用且序号大于 `last_seq` 的**最新**一帧返回；`VideoFrame::data` 指向 mmap 缓冲区，`length` 为有效字节数。处理完不再需要该帧时释放 `shared_ptr`，由删除器触发底层 `QBUF`。

### 3.4 关闭设备

- **`stop()`**（析构函数也会调用）：将 `running_` 置为 `false` 并 `notify_all`，再 `stream_off()`（`VIDIOC_STREAMOFF`）、`join` 采集线程、`unmap_all()` 解除全部 mmap、最后 `close_fd()` 关闭设备文件描述符。
- **运行中若在 `init_v4l2_capture()` 任一环节失败**：同样会先 `stream_off()`，再 `unmap_all()`，`close_fd()`。

## 4. 实现原理

### 4.1 生产模型

- 生产者线程在 `produce_loop()` 中通过 `VIDIOC_DQBUF` 获取一帧。
- 为每帧生成递增序列号 `seq`，封装为 `video_frame`。
- 每帧放入有界队列 `queue_`，并记录该帧当前“待处理消费者集合”。
- 若队列超过 `max_capacity`，直接 `pop_front()` 丢弃最老帧。

### 4.2 多消费者模型

队列元素是 `queued_frame`：

- `frame`：实际帧对象；
- `pending_consumers`：尚未完成该帧（消费或跳过）的消费者 ID 集合。

消费者调用 `wait_frame(consumer_id, last_seq)` 时：

1. 先等待“队列中出现该消费者还未处理且 `seq > last_seq` 的帧”；
2. 为降低延迟，选择该消费者当前可见的最新帧；
3. 将该消费者在该帧及之前帧上的状态从 pending 中移除（之前未取到的帧等价于跳过）；
4. 返回选中的最新帧。

因此，一个帧可以被多个消费者读取；每个消费者的进度独立维护，互不覆盖。

### 4.3 回收模型

`video_frame` 内含 `v4l2_ref`（带自定义 deleter 的 shared_ptr）：

- 当帧从队列中移除，且没有其他消费者持有该帧引用时；
- `v4l2_ref` 的 deleter 会执行 `VIDIOC_QBUF`，把底层 buffer 归还给驱动。

该机制保证 buffer 生命周期与帧实际引用生命周期一致。

## 5. 帧清理场景

当前实现中，帧会在以下场景被清理：

1. **所有消费者已完成（消费或跳过）**  
   `pending_consumers` 为空，帧从队列移除。

2. **队列超容量**  
   队列长度超过 `max_capacity`，最老帧被直接丢弃。

3. **消费者注销**  
   被注销消费者会从所有帧的 `pending_consumers` 中移除；可能导致部分帧立即满足“已完成”并被清理。

4. **模块停止后引用释放**  
   停止采集后，队列帧和消费者持有引用逐步释放，最终触发 buffer 回收。


