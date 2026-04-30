# 视频捕获技术方案


## 1. 模块定位

视频捕获模块通过 V4L2 从设备（如 `/dev/video0`）采集 `YUYV` 帧，采用**单生产者 + 多消费者**模型：

- 生产者线程从驱动 `DQBUF` 取帧并入分发队列
- 多消费者按各自进度批量取帧
- 每帧的底层 V4L2 buffer 在最后一个引用释放时自动 `QBUF` 归还驱动

## 2. 对外接口与语义

### 2.1 `VideoCaptureProvider`

- `VideoCaptureProvider(std::string device, int capture_fps, int capture_width, int capture_height, size_t max_capacity = 10)`
  - 记录采集参数与队列容量上限
  - `consumer_id` 由 `next_consumer_id_` 生成，当前实现从 `1` 开始

- `void start()`
  - 通过 `running_.exchange(true)` 防重复启动
  - 首次调用创建采集线程并进入 `produce_loop()`

- `void stop()`
  - 设置 `running_ = false` 并唤醒等待消费者
  - 执行 `stream_off()`
  - `join` 采集线程（若可 join）
  - `unmap_all()` + `close_fd()` 回收设备资源

- `consumer_id_t register_consumer()`
  - 分配新消费者 ID，加入 `consumers_`

- `void unregister_consumer(consumer_id_t consumer_id)`
  - 从 `consumers_` 移除
  - 清理其在队列各帧上的 pending 标记
  - 触发空节点回收并 `notify_all`

- `std::vector<std::shared_ptr<VideoFrame>> wait_frame(consumer_id_t consumer_id, uint64_t last_seq)`
  - 阻塞等待满足条件的帧（`seq > last_seq` 且该消费者仍在该帧 pending 集合）
  - 返回**所有匹配帧**（按队列顺序）
  - 返回后将这些匹配帧标记为该消费者已处理，并回收已无 pending 的队列节点
  - 若 provider 停止、队列为空、消费者无效或无匹配帧，返回空列表

### 2.2 `ConsumerBase`

- 构造时自动 `register_consumer()`
- `run()` 循环：
  - `wait_frame(consumer_id_, last_seq_)`
  - 空列表则退出
  - 调用派生类 `process(frames)`
  - `last_seq_ = frames.back()->seq`
- 析构时自动 `unregister_consumer()`

## 3. V4L2 采集流程

`produce_loop()` 的实际流程如下：

1. 调 `init_v4l2_capture()` 初始化设备链路  
   失败则 `running_ = false`，唤醒等待者并退出线程。

2. `init_v4l2_capture()` 具体顺序：
   - `close_fd()`（防止旧句柄残留）
   - `open(device_.c_str(), O_RDWR)`
   - `configure_v4l2_capture()`：
     - 校验 `fd_ >= 0` 且 `capture_fps_/width/height > 0`
     - `VIDIOC_S_FMT` 设置 `V4L2_PIX_FMT_YUYV` 与分辨率
     - `VIDIOC_S_PARM` 设置 `timeperframe = 1 / capture_fps`
   - `request_and_map_buffers()`：
     - `VIDIOC_REQBUFS` 申请 `V4L2_MEMORY_MMAP` 缓冲区，默认请求 4 块（至少 2 块）
     - 对每个 index 执行 `VIDIOC_QUERYBUF + mmap`
   - `queue_all_buffers()`：全部 `VIDIOC_QBUF`
   - `stream_on()`：`VIDIOC_STREAMON`

3. 采集循环（`while (running_)`）：
   - `VIDIOC_DQBUF` 取出一帧；失败时 `yield()` 后重试
   - 构造 `VideoFrame`：
     - `seq = ++global_seq_`（从 1 开始递增）
     - `data = mmap_buffers_[buf.index].start`
     - `length = buf.bytesused`
   - 绑定 `v4l2_ref` 自定义删除器：最后引用释放时执行 `VIDIOC_QBUF` 归还该 `buf.index`
   - 入队并复制当前 `consumers_` 为该帧的 `pending_consumers`
   - 若队列超 `max_capacity_`，循环 `pop_front()` 丢弃最老帧
   - `notify_all()` 唤醒等待消费者

## 4. 分发与回收机制

队列节点 `queued_frame` 包含：

- `frame: shared_ptr<VideoFrame>`
- `pending_consumers: unordered_set<consumer_id_t>`

语义是“**批量拉取 + 批量确认**”：

- 拉取：返回该消费者所有 `seq > last_seq` 且仍 pending 的帧
- 确认：`cleanup_consumer_pending_locked(consumer_id, last_seq, false)` 将命中帧上的该消费者状态移除
- 回收：当某节点 `pending_consumers` 为空时，从队列删除

底层 buffer 回收依赖 `VideoFrame::v4l2_ref`：

- 队列删除帧并不一定立刻 `QBUF`
- 只有该帧 `shared_ptr` 全部释放（包括消费者侧临时引用）后，删除器才执行 `VIDIOC_QBUF`

这保证了“驱动 buffer 生命周期”和“业务持有帧生命周期”一致。

## 5. 关闭与异常路径

- 正常关闭：`stop()` 完整执行 `running=false -> notify -> STREAMOFF -> join -> unmap -> close`
- 初始化任一步失败：`init_v4l2_capture()` 内部执行 `stream_off -> unmap_all -> close_fd` 后返回失败
- Provider 析构函数会调用 `stop()`，但业务侧仍建议显式停机，便于控制退出顺序

## 6. CLI 示例（`preview-video`）

`project/agent/cli/preview-video/main.cpp` 的典型使用：

1. 构造 `VideoCaptureProvider(device, 30, 640, 480, 30)`
2. `provider->start()`
3. 构造 `FrameViewerConsumer` 并 `run()`
4. 退出时 `provider->stop()`

`FrameViewerConsumer::process()` 当前是“取批量中的最后一帧显示”（`frames.back()`），用于降低显示延迟。


