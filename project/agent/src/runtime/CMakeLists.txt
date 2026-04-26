add_library(pi_guard_runtime STATIC
    app/agent_app.cpp
)

target_include_directories(pi_guard_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_runtime
    PUBLIC
        pi_guard_foundation
        pi_guard_audio_capture
        pi_guard_video_capture
        pi_guard_echo_canceller
        pi_guard_encoder
        pi_guard_motion_detect
        pi_guard_output_audio
        pi_guard_output_file
        pi_guard_config_manager
        pi_guard_event_bus
        pi_guard_infra_log
        pi_guard_perf_monitor
        pi_guard_http_notify
        pi_guard_rtmp_pusher
        pi_guard_websocket_client
)
