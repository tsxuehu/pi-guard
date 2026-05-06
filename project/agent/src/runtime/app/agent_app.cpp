#include "agent_app.hpp"

#include <chrono>

namespace piguard::app {

AgentApp::AgentApp(std::string config_path)
    : config_manager_(std::move(config_path)),
      audio_capture_(audio_queue_),
      motion_detect_(video_queue_, event_queue_),
      encoder_(nullptr, nullptr),
      file_writer_(event_queue_) {}

AgentApp::~AgentApp() { stop(); }

bool AgentApp::start() {
    if (running_.exchange(true)) {
        return true;
    }

    log_module_.start();
    logger_ = infra_log::LogFactory::getLogger("AgentApp");
    config_manager_.start();
    perf_monitor_.start();
    video_capture_.start();
    audio_capture_.start();
    motion_detect_.start();
    encoder_.start();
    echo_canceller_.start();
    file_writer_.start();
    audio_playback_.start();
    rtmp_pusher_.start();
    http_notify_.start();
    ws_client_.start();

    event_bus_.subscribe(foundation::EventType::MotionStart, [this](const foundation::Event& event) {
        logger_->info("motion detected: " + event.payload);
    });

    start_threads();
    return true;
}

void AgentApp::stop() {
    if (!running_.exchange(false)) {
        return;
    }

    stop_threads();
    ws_client_.stop();
    http_notify_.stop();
    rtmp_pusher_.stop();
    audio_playback_.stop();
    file_writer_.stop();
    echo_canceller_.stop();
    encoder_.stop();
    motion_detect_.stop();
    audio_capture_.stop();
    video_capture_.stop();
    perf_monitor_.stop();
    config_manager_.stop();
    log_module_.stop();
}

void AgentApp::run_for_demo() {
    using namespace std::chrono_literals;
    for (int i = 0; i < 10 && running_.load(); ++i) {
        std::this_thread::sleep_for(100ms);
        auto stats = perf_monitor_.collect();
        if (logger_ != nullptr) {
            logger_->debug("perf cpu=" + std::to_string(stats.cpu_usage_percent) +
                           ", mem=" + std::to_string(stats.memory_usage_percent) +
                           ", temp=" + std::to_string(stats.temperature_celsius));
        }
    }
}

void AgentApp::start_threads() {
    using namespace std::chrono_literals;

    workers_.emplace_back([this]() {
        while (running_.load()) {
            // video_capture_.poll_once();
            std::this_thread::sleep_for(33ms);
        }
    });

    workers_.emplace_back([this]() {
        while (running_.load()) {
            // audio_capture_.poll_once();
            std::this_thread::sleep_for(20ms);
        }
    });

    workers_.emplace_back([this]() {
        while (running_.load()) {
            motion_detect_.process_once();
            const auto event = event_queue_.pop();
            if (event.has_value()) {
                event_bus_.publish(*event);
                event_queue_.push(*event);
            }
            std::this_thread::sleep_for(66ms);
        }
    });

    workers_.emplace_back([this]() {
        while (running_.load()) {
            file_writer_.flush_once();
            std::this_thread::sleep_for(50ms);
        }
    });

    workers_.emplace_back([this]() {
        while (running_.load()) {
            ws_client_.poll_once();
            http_notify_.notify_once();
            rtmp_pusher_.push_once();
            encoder_.encode_once();
            echo_canceller_.process_once();
            audio_playback_.play_once();
            std::this_thread::sleep_for(30ms);
        }
    });
}

void AgentApp::stop_threads() {
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

}  // namespace piguard::app
