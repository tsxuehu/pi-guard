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
#include "rtmp_pusher/rtmp_publisher_module.hpp"
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

};

}  // namespace piguard::app
