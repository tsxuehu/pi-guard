#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include "capture_audio/audio_capture_module.hpp"
#include "capture_video/video_capture_module.hpp"
#include "foundation/thread_safe_queue.hpp"
#include "foundation/types.hpp"
#include "access_http/http_notify.hpp"
#include "access_pusher/rtmp_pusher.hpp"
#include "access_websocket/websocket_client.hpp"
#include "infra_config/config_manager.hpp"
#include "infra_event/event_bus.hpp"
#include "infra_log/logger_factory.hpp"
#include "infra_log/log_module.hpp"
#include "infra_monitor/perf_monitor.hpp"
#include "output_audio/audio_playback.hpp"
#include "output_file/file_writer.hpp"
#include "processing_echo_canceller/echo_canceller.hpp"
#include "processing_encoder/encoder.hpp"
#include "processing_motion_detect/motion_detect.hpp"

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
    infra_log::LogModule log_module_;
    std::shared_ptr<infra_log::Logger> logger_;
    infra::PerfMonitor perf_monitor_;
    infra::EventBus event_bus_;

    capture_video::VideoCaptureModule video_capture_;
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
