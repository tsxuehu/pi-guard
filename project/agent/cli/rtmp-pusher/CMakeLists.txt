add_executable(rtmp-pusher-demo
    main.cpp
)

target_link_libraries(rtmp-pusher-demo
    PRIVATE
        pi_guard_foundation
        pi_guard_infra_log
        pi_guard_video_capture
        pi_guard_audio_capture
        pi_guard_encoder
        pi_guard_rtmp_pusher
)
