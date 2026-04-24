add_library(pi_guard_capture STATIC
    audio/audio_capture.cpp
    video/video_capture.cpp
)

target_include_directories(pi_guard_capture
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_capture
    PUBLIC
        pi_guard_foundation
)
