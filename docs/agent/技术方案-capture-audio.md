# 音频捕获技术方案


## 1. 模块定位

音频捕获模块通过 ALSA 持续采集 PCM（`S16_LE`、交织布局），采用**单生产者 + 多消费者**模型：

- 生产者线程：`AudioCaptureProvider::produce_loop()`
- 分发队列：`std::list<queued_audio>`
- 消费者：继承 `AudioConsumerBase`，通过 `wait_audio()` 批量获取待处理帧

## 2. 对外接口与语义

### 2.1 `AudioCaptureProvider`

- `AudioCaptureProvider(std::string device, unsigned sample_rate_hz, unsigned channels)`
  - `device` 不能为空
  - `sample_rate_hz` 必须 `> 0`
  - `channels` 必须 `>= 1`
  - 参数非法时抛 `std::invalid_argument`

- `bool start()`
  - 通过 `running_.exchange(true)` 防止重复启动
  - 首次调用会创建采集线程并返回 `true`
  - 已运行时直接返回 `true`

- `void stop()`
  - 将 `running_` 置 `false` 并 `notify_all`
  - 若采集线程可 `join`，在 `stop()` 内部直接等待线程退出
  - 不清空分发队列（队列由消费/回收逻辑自然清理）

- `consumer_id_t register_consumer()`
  - 返回从 `0` 递增的消费者 ID
  - 将 ID 加入 `active_consumers_`

- `void unregister_consumer(consumer_id_t id)`
  - 从活跃集合移除
  - 清理该 ID 在队列各节点的 `pending_consumers`
  - 对 `pending_consumers` 为空的节点立即回收

- `std::vector<std::shared_ptr<audio_frame>> wait_audio(consumer_id_t id, uint64_t last_seq)`
  - 阻塞等待满足条件的帧：`seq > last_seq` 且该帧仍待该消费者处理
  - 返回**所有命中的帧列表**（按队列顺序）
  - 返回后会把这些帧上的该消费者标记为“已处理”，并回收无人待处理节点
  - 以下情况返回空列表：`running_ == false`、队列空、消费者已失效、或无匹配帧

### 2.2 `AudioConsumerBase`

- 构造时自动 `register_consumer()`
- `run()` 循环：
  - 调 `wait_audio(consumer_id_, last_seq_)`
  - 若返回空列表则退出
  - 调派生类 `process(frames)`
  - `last_seq_ = frames.back()->seq`
- 析构时自动 `unregister_consumer()`

## 3. 采集线程（ALSA）流程

`produce_loop()` 主流程如下：

1. `snd_pcm_open(..., SND_PCM_STREAM_CAPTURE, 0)` 打开设备  
   失败则记录日志，`running_ = false` 并唤醒等待者后退出。

2. `snd_pcm_set_params(...)` 配置：
   - 格式：`SND_PCM_FORMAT_S16_LE`
   - 访问：`SND_PCM_ACCESS_RW_INTERLEAVED`
   - 声道：`channels_`
   - 采样率：`sample_rate_`
   - 允许重采样：`1`
   - 目标延迟：`50000` 微秒

3. 尝试开启非阻塞：`snd_pcm_nonblock(handle, 1)`（失败仅告警，不中断）

4. 计算单次读取帧数（约 20ms）：
   - `frame_size = sample_rate_ * 20 / 1000`（防御性兜底为 `320`）

5. 循环读音频：
   - `snd_pcm_readi(handle, buffer.data(), frame_size)`
   - `-EPIPE`：`snd_pcm_prepare(handle)` 后继续
   - `-EAGAIN`：sleep 5ms 后继续（便于及时响应 stop）
   - 其他负值：记录错误，sleep 1ms 后继续
   - 正常读到 `frames > 0`：
     - 构造 `audio_frame`
     - `seq = next_seq_++`
     - `pcm_data` 拷贝 `frames * channels_` 个 `int16_t`
     - `timestamp = steady_clock::now().time_since_epoch().count()`
     - 入队并唤醒等待者

6. 队列容量控制：
   - `max_queue_capacity_ = 50`
   - 超限后 `pop_front()` 丢弃最旧节点

7. 退出循环后关闭 PCM：`snd_pcm_close(handle)`

## 4. 队列与多消费者分发机制

队列节点 `queued_audio` 包含：

- `frame: shared_ptr<audio_frame>`
- `pending_consumers: set<consumer_id_t>`（还没处理该帧的消费者）

新帧入队时，`pending_consumers` 直接复制当前 `active_consumers_` 快照。  
因此同一帧可被多个消费者独立消费，且消费进度互不覆盖。

`wait_audio()` 的消费语义是“**批量拉取并批量确认**”：

- 拉取：把当前队列内对该消费者“`seq > last_seq` 且待处理”的帧全部收集返回
- 确认：通过 `cleanup_consumer_pending_locked(id, last_seq, false)` 将这些匹配帧上的该消费者状态移除
- 回收：任何 `pending_consumers` 变空的节点都会被立即 `erase`

## 5. 线程生命周期约定

当前实现由 `stop()` 统一完成停机：

- 发停止信号（`running_ = false`）
- 唤醒等待中的消费者（`notify_all`）
- 若采集线程可等待，则在 `stop()` 内 `join`

`record-audio` CLI 的实际关闭顺序如下：

1. `provider->stop()`
2. 等待消费者线程 `join`

建议业务侧遵循同样顺序，避免消费者线程尚未退出就提前析构相关对象。

## 6. `audio_frame` 数据约定

`audio_frame` 字段：

- `seq`：全局递增序号
- `pcm_data`：`S16_LE` 交织样本，长度为本次读取的 `frames * channels`
- `timestamp`：`steady_clock` 计数值（单位取决于 `count()` 的底层周期）

补充说明：

- `audio_frame` 内不存 `channels/sample_rate`；消费方需和 provider 参数保持一致
- 多声道布局为交织（例如双声道：`L0, R0, L1, R1, ...`）
- 由于队列节点持 `shared_ptr`，帧内存在最后一个引用释放后自动回收
