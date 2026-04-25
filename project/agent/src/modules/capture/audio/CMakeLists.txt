add_library(pi_guard_audio_capture STATIC
    audio_capture.cpp
)

target_include_directories(pi_guard_audio_capture
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_audio_capture
    PUBLIC
        pi_guard_foundation
)
