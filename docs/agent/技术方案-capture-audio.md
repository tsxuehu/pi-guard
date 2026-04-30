# 音频捕获模块技术说明

## 1. 模块功能

音频捕获模块用于通过 **ALSA** 从麦克风等采集设备持续读取 PCM，并将数据以固定时间片分包后，分发给多个消费者线程。与视频捕获模块同属「单生产者、多消费者」模型，模块目标如下：

- 支持单生产者、多消费者并发访问。
- 消费者可与采集节拍对齐：生产者每 enqueue 一包，消费者即通过 `wait_audio` 可取到并重放处理（或由 `wait_audio` 自动跳到队列中对该消费者可用的最新一包）。
- 同一包可被多个消费者各自独立消费（各自的 `consumer_id`、`last_seq` 独立）。
- 队列有容量上限（`max_queue_capacity_`，当前约可容纳约 1 秒分包），超限时丢弃最老包，压低延迟。
- PCM 拷贝进 `audio_frame::pcm_data`，不依赖 mmap 环形缓冲回收；队列与 `shared_ptr` 生命周期决定何时释放内存。

源码主要路径：`project/agent/src/modules/capture/audio/`，帧类型见 `include/capture_audio/audio_frame.hpp`，分发器见 `audio_capture_provider.hpp`。

## 2. 对外接口

主要由 `AudioCaptureProvider` 提供对外能力：

- **构造 `AudioCaptureProvider(std::string device, unsigned int sample_rate_hz, unsigned int channels)`**（无默认参数）  
  指定 ALSA 设备名（如 `"default"`、`"plughw:0,7"`）、采样率（须 `> 0`）、声道数（须 `>= 1`）。设备名为空或不满足上述约束时抛出 `std::invalid_argument`。后续 `produce_loop()` 中 `snd_pcm_set_params` 与单次读取的帧数均由这些参数推导。

- `bool start()`  
  启动采集线程（重复调用语义与实现一致：已通过 `exchange` 避免二次启动语义混乱；业务上建议只调用一次）。

- `void stop()`  
  停止线程、`join`、`clear` 内部分发包队列并 `notify_all`。

- `consumer_id_t register_consumer()`  
  注册消费者，返回 ID（从 0 递增），新入队的包会将当前活跃消费者集写入对应 `pending_consumers`。

- `void unregister_consumer(consumer_id_t id)`  
  将该 ID 从活跃集合与各队列元素的 `pending_consumers` 中移除；若某元素 `pending_consumers` 为空则从队列摘除。

- `std::shared_ptr<audio_frame> wait_audio(consumer_id_t id, uint64_t last_seq)`  
  阻塞直到有可用的、对该消费者尚未处理且 `seq > last_seq` 的包；为降低延迟，选取**当前队列中对该消费者可用的最新一包**返回，并将该消费者在该包之前所有包上等价为「跳过」并从队列中摘除（已无待处理消费者的元素）。

消费者基类 `AudioConsumerBase`（`capture_audio/consumer_base.hpp`）约定：

- 构造时传入 `provider` 与固定用途的 **消费者名称**（`std::string`，日志等用），内部自动 `register_consumer()`；
- `run()` 中循环：`wait_audio` → `process(frame)` → 更新 `last_seq_`，与生产者分包频率一致地完成消费；
- 析构时 `unregister_consumer`。

构建依赖中与 ALSA 相关的系统包见仓库内 `docs/agent/构建.pd`（如 `libasound2-dev`）。

## 3. ALSA 设备操作流程

底层实现在 `AudioCaptureProvider::produce_loop()` 中完成；业务侧一般只需 **构造传入设备名与格式** → **`start()`** → **`register_consumer()` + `wait_audio()`** → **`stop()`**。与代码一致的主要步骤如下。

### 3.1 打开设备

- `start()` 将 `running_` 置为真并启动线程后，线程内调用 `snd_pcm_open(&handle, device_.c_str(), SND_PCM_STREAM_CAPTURE, 0)`。  
- 打开失败：`running_ = false`，线程直接返回（等待端可通过 `wait_audio` 与 `running_` 结合观察结束）。

### 3.2 配置与参数

- 使用 `snd_pcm_set_params` 配置为：  
  - 格式：`SND_PCM_FORMAT_S16_LE`  
  - 访问：`SND_PCM_ACCESS_RW_INTERLEAVED`（交织多声道）  
  - 声道数、采样率：来自构造参数 `channels_`、`sample_rate_`  
  - 其余参数与实现中软重采样标志、期望延迟（如 `50000` 微秒档）与官方 API 约定一致，以源码为准。

### 3.3 读取与封装为 `audio_frame`

- **单次读取帧数（约 20ms）**  
  `frame_size = sample_rate_ * 20 / 1000`（若 `sample_rate_` 异常回退为 320，与历史 16kHz/20ms 兼容）。  
- 循环 `snd_pcm_readi(handle, buffer.data(), frame_size)`：  
  - `-EPIPE`：`snd_pcm_prepare` 后重试；  
  - 其他负值：短暂 `sleep` 后重试；  
  - 成功：将 `frames * channels_` 个 `int16_t` 拷入 `audio_frame::pcm_data`，填写递增 `seq` 与 `timestamp`（`steady_clock` 的 `time_since_epoch().count()`），再入队并 `notify_all`。

### 3.4 关闭设备

- `produce_loop` 在 `running_` 为假退出循环后执行 `snd_pcm_close(handle)`。  
- `stop()` 侧将 `running_` 置假、`join` 线程，并在持锁下 `queue_.clear()`，唤醒所有 `wait_audio` 等待者。

## 4. 实现原理

### 4.1 生产模型

- 单采集线程在 `produce_loop()` 中阻塞/重试式 `read`，每读满一片即分配一个 `audio_frame`（`shared_ptr`），PCM 已复制到 `std::vector<int16_t>`。  
- 每包带全局递增 `seq`。  
- 队列为 `std::list<queued_audio>`，元素包含 `frame` 与 `pending_consumers`（`std::set<consumer_id_t>`）。新包入队时 `pending_consumers` 初始为当前 `active_consumers_` 快照。  
- 若 `queue_.size() > max_queue_capacity_`，`pop_front()` 丢弃最老包（与视频侧「有界队列丢旧」一致）。

### 4.2 多消费者模型

与视频模块思想一致：每个消费者维护自己的 `last_seq`，调用 `wait_audio(id, last_seq)` 时：

1. 条件变量等待「存在 `seq > last_seq` 且该 `id` 仍在该元素 `pending_consumers` 中」的包；  
2. 从队列**从后向前**查找满足条件的**最新**一包（`find_latest_frame_locked`）；  
3. 从队头到该包之前，将该 `id` 从各元素 `pending_consumers` 中擦除；若某元素无任何待处理消费者则 `erase` 该元素；  
4. 返回选中包的 `shared_ptr`。

因此：慢消费者会「跳过」中间包，只拿到最新可见包；多消费者互不影响各自进度。与视频不同的是，**没有驱动层 buffer 的 QBUF**，仅有堆上 `vector` 与 `shared_ptr` 引用计数。

### 4.3 回收模型

- 包从队列移除后，若不再有 `shared_ptr<audio_frame>` 引用，则 `pcm_data` 与 `audio_frame` 本身由标准库释放。  
- `AudioConsumerBase` 析构时在 `provider_` 非空情况下会调用 `unregister_consumer(consumer_id_)`。**`consumer_id_t` 自 `0` 起均为合法 ID**（与 `AudioCaptureProvider::register_consumer()` 返回值一致）。

## 5. `audio_frame` 与清理场景

`audio_frame`（独立头文件 `audio_frame.hpp`）字段含义：

| 字段 | 含义 |
|------|------|
| `seq` | 全局递增序号，用于消费者 `last_seq` 推进与选包。 |
| `pcm_data` | S16_LE 交织样本；长度为 `frames * channels`（与本次 `read` 实际帧数一致）。 |
| `timestamp` | 采集时刻的 `steady_clock` 时间戳计数值（单位与 `count()` 一致，业务若要对齐视频时间线需自行约定换算）。 |

**`audio_frame` 自身不携带声道数**；声道数与构造 `AudioCaptureProvider` 时传入的 `channels` 一致，消费端需与 Provider 约定对齐（例如单写死为 1 声道，或上层配置与 WAV/编码器元数据一致）。

### 5.1 PCM 数据存放（格式与多声道交织）

- **样本类型**：固定为 **有符号 16 位小端**（`S16_LE`），与 ALSA `SND_PCM_FORMAT_S16_LE` 一致。  
- **内存布局**：`pcm_data` 为 **一维** `std::vector<int16_t>`，采用 **`SND_PCM_ACCESS_RW_INTERLEAVED`（交织访问）**，即 ALSA 单次 `snd_pcm_readi` 返回缓冲区在内存中的自然顺序，**未做按声道拆平面**（无独立的 L/R 缓冲区）。  
- **「帧」（frame）语义**：在 ALSA 中表示**同一时刻全部声道各一个采样**；`snd_pcm_readi` 的第三个参数 **`frame_size`** 是**帧数**，单次成功读取后用户态有效元素个数为 **`frames * channels`**（`frames` 为本次返回值，可能小于请求的 `frame_size`）。  
- **多声道下标标系**（以立体声 `channels = 2` 为例）：线性下标与时间的对应关系为  
  `[0]=ch0_t0, [1]=ch1_t0, [2]=ch0_t1, [3]=ch1_t1, …`。一般声卡上 **ch0/ch1 对应左/右声道**（仍以设备/驱动为准）。  
- **单声道**：等价于仅按时间排列的 `int16_t` 序列，长度为 `frames * 1`。  
- **写文件**：标准 WAV 对多声道 PCM 同样要求 **交织**，本仓库 `record-audio` 在头里写入的 `channels`、`sample_rate` 与上述 `pcm_data` 布局一致即可被常见播放器正确解码。

### 5.2 包从队列消失或被丢弃

包会在以下场景从队列消失或被丢弃：

1. **某包对该消费者已处理或已跳过**  
   `wait_audio` 在返回最新包的路径上，会将该消费者从更早包的 `pending_consumers` 中移除；若某包不再被任何消费者等待，则从 `list` 中擦除。

2. **队列超容量**  
   超过 `max_queue_capacity_` 时丢弃队头最老包。

3. **消费者注销**  
   `unregister_consumer` 将该 ID 从所有元素的 `pending_consumers` 中删除，并删除已无待处理消费者的元素。

4. **停止**  
   `stop()` 清空队列；已返回给业务层的 `shared_ptr` 仍有效直至最后持有者释放。
