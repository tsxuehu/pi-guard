add_library(pi_guard_output STATIC
    audio/audio_playback.cpp
    file/file_writer.cpp
)

target_include_directories(pi_guard_output
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_output
    PUBLIC
        pi_guard_foundation
)
