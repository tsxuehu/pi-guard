# rtmp-pusher-demo

一个最小 RTMP 推流示例：从本地音视频采集，经过 `processing_encoder::Encoder` 编码后，推送到 RTMP 服务器（如 SRS）。

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

## 2. 运行推流

```bash
./build/cli/rtmp-pusher/rtmp-pusher-demo
```

可选参数：

```bash
./build/cli/rtmp-pusher/rtmp-pusher-demo <video_device> <audio_device> <rtmp_url>
```

例如：

```bash
./build/cli/rtmp-pusher/rtmp-pusher-demo /dev/video0 hw:1,0 rtmp://127.0.0.1/live/livestream
```

## 3. 拉流验证

使用 `ffplay`：

```bash
ffplay rtmp://127.0.0.1/live/livestream
```

或使用 VLC 直接打开同一个 RTMP URL。

## 4. 停止

- 推流端按 `Ctrl+C`
- SRS 容器按 `Ctrl+C`

## 5. 常见问题

### 5.1 SRS 报 HlsDecode / `not annexb`，推流端 `Broken pipe`

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

- 若日志里出现 `pts=-9223372036854775808`（即 FFmpeg 的 `AV_NOPTS_VALUE`），说明某路包 **没有合法时间戳**，FLV 时间轴会乱，可能加重服务端异常表现。`RtmpPusher` 会对无效 PTS/DTS 做 **单调递增补齐**；若仍频繁出现，应检查编码器是否正常写出 `pts`/`dts`。
