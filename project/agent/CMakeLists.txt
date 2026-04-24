cmake_minimum_required(VERSION 3.16)

project(pi_guard_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/external_access)

add_executable(pi-guard-agent
    src/app/agent_app.cpp
    src/capture/audio/audio_capture.cpp
    src/capture/video/video_capture.cpp
    ${PI_GUARD_AGENT_EXTERNAL_ACCESS_SOURCES}
    src/infra/config/config_manager.cpp
    src/infra/event_bus/event_bus.cpp
    src/infra/log/logger.cpp
    src/infra/perf_monitor/perf_monitor.cpp
    src/main.cpp
    src/output/audio/audio_playback.cpp
    src/output/file/file_writer.cpp
    src/processing/echo_canceller/echo_canceller.cpp
    src/processing/encoder/encoder.cpp
    src/processing/motion_detect/motion_detect.cpp
)

target_include_directories(pi-guard-agent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

install(TARGETS pi-guard-agent
    RUNTIME DESTINATION bin
)
