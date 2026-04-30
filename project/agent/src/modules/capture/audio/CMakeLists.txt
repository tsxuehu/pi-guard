find_package(ALSA REQUIRED)

add_library(pi_guard_audio_capture STATIC
    audio_capture_module.cpp
    audio_capture_provider.cpp
)

target_include_directories(pi_guard_audio_capture
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE
        ${ALSA_INCLUDE_DIR}
)

target_link_libraries(pi_guard_audio_capture
    PUBLIC
        pi_guard_foundation
        pi_guard_infra_log
        ${ALSA_LIBRARIES}
)
