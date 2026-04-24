#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "capture/audio/audio_capture.hpp"
#include "capture/video/video_capture.hpp"
#include "core/thread_safe_queue.hpp"
#include "core/types.hpp"
#include "external_access/http/http_notify.hpp"
#include "external_access/pusher/rtmp_pusher.hpp"
#include "external_access/websocket/websocket_client.hpp"
#include "infra/config/config_manager.hpp"
#include "infra/event_bus/event_bus.hpp"
#include "infra/log/logger.hpp"
#include "infra/perf_monitor/perf_monitor.hpp"
#include "output/audio/audio_playback.hpp"
#include "output/file/file_writer.hpp"
#include "processing/echo_canceller/echo_canceller.hpp"
#include "processing/encoder/encoder.hpp"
#include "processing/motion_detect/motion_detect.hpp"

namespace piguard::app {

class AgentApp {
public:
    explicit AgentApp(std::string config_path);
    ~AgentApp();

    bool start();
    void stop();
    void run_for_demo();

private:
    void start_threads();
    void stop_threads();

    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;

    core::ThreadSafeQueue<core::VideoFrame> video_queue_;
    core::ThreadSafeQueue<core::AudioFrame> audio_queue_;
    core::ThreadSafeQueue<core::Event> event_queue_;

    infra::ConfigManager config_manager_;
    infra::Logger logger_;
    infra::PerfMonitor perf_monitor_;
    infra::EventBus event_bus_;

    capture::VideoCapture video_capture_;
    capture::AudioCapture audio_capture_;

    processing::MotionDetect motion_detect_;
    processing::Encoder encoder_;
    processing::EchoCanceller echo_canceller_;

    output::FileWriter file_writer_;
    output::AudioPlayback audio_playback_;

    external_access::RTMPPusher rtmp_pusher_;
    external_access::HTTPNotify http_notify_;
    external_access::WebSocketClient ws_client_;
};

}  // namespace piguard::app
