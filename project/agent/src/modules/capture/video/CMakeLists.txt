add_library(pi_guard_video_capture STATIC
    video_capture.cpp
    video_capture_provider.cpp
)

target_include_directories(pi_guard_video_capture
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_video_capture
    PUBLIC
        pi_guard_foundation
        pi_guard_infra_log
)
