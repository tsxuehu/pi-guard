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
