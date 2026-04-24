#pragma once

#include <atomic>
#include <string>
#include <thread>
#include <vector>

#include "audio_capture.hpp"
#include "video_capture.hpp"
#include "thread_safe_queue.hpp"
#include "types.hpp"
#include "http_notify.hpp"
#include "rtmp_pusher.hpp"
#include "websocket_client.hpp"
#include "config_manager.hpp"
#include "event_bus.hpp"
#include "logger.hpp"
#include "perf_monitor.hpp"
#include "audio_playback.hpp"
#include "file_writer.hpp"
#include "echo_canceller.hpp"
#include "encoder.hpp"
#include "motion_detect.hpp"

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

    foundation::ThreadSafeQueue<foundation::VideoFrame> video_queue_;
    foundation::ThreadSafeQueue<foundation::AudioFrame> audio_queue_;
    foundation::ThreadSafeQueue<foundation::Event> event_queue_;

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
