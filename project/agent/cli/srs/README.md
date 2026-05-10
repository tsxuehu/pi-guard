# srs-push-demo

一个最小 RTMP 推流示例：从本地音视频采集，经过 `processing_encoder::Encoder` 编码后，推送到 SRS。

## 1. 启动 SRS

如果你本机有 Docker，可以直接：

```bash
docker run --rm -it \
  -p 1935:1935 \
  -p 1985:1985 \
  -p 8080:8080 \
  ossrs/srs:5
```

默认推流地址为：

`rtmp://127.0.0.1/live/livestream`

## 2. 构建示例

在 `project/agent` 目录下：

```bash
cmake -S . -B build
cmake --build build --target srs-push-demo -j4
```

## 3. 运行推流

```bash
./build/cli/srs/srs-push-demo
```

可选参数：

```bash
./build/cli/srs/srs-push-demo <video_device> <audio_device> <rtmp_url>
```

例如：

```bash
./build/cli/srs/srs-push-demo /dev/video0 hw:1,0 rtmp://127.0.0.1/live/livestream
```

## 4. 拉流验证

使用 `ffplay`：

```bash
ffplay rtmp://127.0.0.1/live/livestream
```

或使用 VLC 直接打开同一个 RTMP URL。

## 5. 停止

- 推流端按 `Ctrl+C`
- SRS 容器按 `Ctrl+C`

## 6. 常见问题：原因与处理

### 6.1 SRS 报 HlsDecode / `not annexb`，推流端 `Broken pipe`

**原因简述**

- RTMP/FLV 里 H.264 常见打包为 **AVCC（IBMF）**：每个 NAL 前是 **4 字节大端长度**，不是 Annex B 的 **`00 00 01` 起始码**。
- SRS 在转 HLS 等路径上会 **启发式判断** 当前负载是 Annex B 还是 AVCC。若某帧里 **长度字段的前几个字节刚好像起始码**（例如 `00 00 01 xx`），可能 **误判成 Annex B**，进入 `do_avc_demux_annexb_format` 后又对不上，于是报 `not annexb`。
- 发布失败后 SRS 会 **断开 RTMP**，客户端继续 `write` 就会得到 **`Broken pipe`**，与本地 TCP 是否稳定无必然关系。

**处理（服务端）**

在对应 `vhost` 的 `publish` 中关闭「先尝试 Annex B」，让 SRS **优先按 AVCC 解析**，例如：

```conf
vhost __defaultVhost__ {
    publish {
        try_annexb_first off;
    }
}
```

具体项名、语法以你所用 **SRS 版本官方文档** 为准。相关讨论：[SRS #3621](https://github.com/ossrs/srs/issues/3621)、[SRS #4112](https://github.com/ossrs/srs/issues/4112)。

**处理（推流端）**

- 若日志里出现 `pts=-9223372036854775808`（即 FFmpeg 的 `AV_NOPTS_VALUE`），说明某路包 **没有合法时间戳**，FLV 时间轴会乱，可能加重服务端异常表现。`SrsRtmpPusher` 会对无效 PTS/DTS 做 **单调递增补齐**；若仍频繁出现，应检查编码器是否正常写出 `pts`/`dts`。

### 6.2 音视频不同步（声音正常、画面明显落后或对不上嘴型）

**原因简述**

- **两套时间基准未对齐**：若视频 PTS 用采集 **帧序号**，音频 PTS 用 **从零累加的采样点**，两边 **不表示同一绝对时间**。编码线程启动快慢、缓冲区堆积程度不同会产生 **固定或可变的声画时差**。
- **只保留最后一帧编码时**：一次拉取多帧只编码 **最新一帧**，音频仍按实时 **连续送出**。画面在时间轴上会 **整块跳秒**，而音频不停，主观上常为 **画面“拖在后面”或长期冻结某一帧**，与「单纯延迟一两帧」不同。

**处理（本仓库已实现）**

- 视频：在 V4L2 出队时写入与音频一致的 **`steady_clock` 时间戳**（`VideoFrame::timestamp_ns`）。
- 编码器：在 `Encoder::start()` 完成初始化后、启动编码线程前记录 **`program_t0_ns`**，视频帧 PTS 按「采集时刻 − t0」换算到 **`1/fps` 时间基**；音频在 PCM 缓冲从空到有数据时记录 **队首样本对应时刻**，首个 AAC 包的 PTS 同样相对 **`program_t0_ns`**，之后仍按 **`+ frame_samples`** 递增，并与队首时间在纳秒尺度上对齐推进。
- 推流侧消费线程对每个批次更新 **`last_seq`**，避免对 `wait_packet` 语义依赖不清。

这样在 **丢帧、线程调度不均** 时，画面时间戳会随 **真实采集时刻** 跳变，与 **按 wall-clock 对齐的音频** 处于同一时间线，可减少「声音走了很久画面还在原地」的现象。若仍存在少量偏差，可再排查播放器首开缓冲、GOP、SRS 转封装缓存等链路。

