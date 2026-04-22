## 系统介绍

pi-guard 是一个面向家庭/轻量工业场景的实时安防系统，整体采用分层架构，核心能力包括实时监控、移动侦测、事件录像、历史回看与双向语音对讲。

系统运行在 Ubuntu 环境上，部署平台支持 ARM 架构的树莓派设备以及 x86-64 架构主机，可根据场景在边缘侧或通用服务器上灵活落地。

系统由四个核心层级组成：

1. 硬件与采集层（树莓派 C++ 程序，`pi-guard-agent`）
   - 通过 V4L2 采集摄像头视频，通过 ALSA 采集麦克风音频。
   - 使用 OpenCV 做移动侦测（灰度化、背景建模、帧差与轮廓过滤）。
   - 在触发移动事件时本地异步录制 MP4（H.264 + AAC）。
   - 基于 FFmpeg API 推送 RTMP 到 SRS（视频使用 `h264_v4l2m2m` 硬件编码）。
   - 通过 WebSocket 与后端保持连接，接收手机下行语音并在本地播放，同时将本地上行音频推送到手机，支持全双工对讲。

2. 流媒体交换层（SRS，`pi-guard-srs`）
   - 接收并转发来自树莓派的 RTMP 音视频流到多个终端。
   - 支持 DVR/HLS 录制（`.m3u8` + `.ts`）作为备份。
   - 通过 HTTP Callback 向 Node.js 通知推流状态与录制事件。

3. 业务逻辑层（Node.js 后端，`pi-guard-server`）
   - 接收与存储移动侦测事件（时间戳、录像路径等）。
   - 提供录像文件的 HTTP 访问与分发。
   - 作为 WebSocket 语音网关，在手机端与树莓派之间透传语音包。
   - 协调对讲会话状态，避免多人同时发言引发音频冲突。

4. 终端交互层（手机 Web 端，`pi-guard-web`）
   - 播放实时流（RTMP/HTTP-FLV/HLS）。
   - 采集手机麦克风音频并通过 WebSocket 发给后端，再转发到树莓派。
   - 支持“按住说话”模式，减少啸叫。
   - 展示移动侦测事件列表并回放对应历史视频片段。

### 关键能力

- 实时监控：摄像头 -> C++ -> SRS -> 手机端播放。
- 移动报警：OpenCV 检测 -> Node.js 记录 -> 手机端提示/列表展示。
- 双向语音：手机与树莓派通过 Node.js WebSocket 网关实时互通。
- 历史回看：本地录像通过 Node.js 文件服务对外提供访问。

### 术语对照

- `pi-guard-agent`：树莓派采集代理（C++）
- `pi-guard-srs`：流媒体服务配置（SRS）
- `pi-guard-server`：业务网关服务（Node.js）
- `pi-guard-web`：移动端 Web 前端
- `pi-guard-db`：事件与业务数据存储（SQLite/MongoDB）
- `piguard.conf`：系统配置文件
- `piguard.service`：systemd 服务定义

## 工程结构说明

业务工程位于 `project/` 目录。以下为推荐的工程组织方式，便于按组件独立开发、测试与部署：

```text
pi-guard/
├── README.md
└── project/
    ├── piguard.conf.example
    ├── deploy/
    │   ├── systemd/
    │   ├── docker/
    │   └── scripts/
    ├── agent/                      # pi-guard-agent（C++）
    ├── server/                     # pi-guard-server（Node.js）
    ├── web/                        # pi-guard-web（移动端页面）
    ├── srs/                        # pi-guard-srs（流媒体配置）
    └── docs/
```