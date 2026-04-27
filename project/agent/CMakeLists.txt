cmake_minimum_required(VERSION 3.16)

project(pi_guard_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/foundation)
add_subdirectory(src/modules/capture/audio)
add_subdirectory(src/modules/capture/video)
add_subdirectory(src/modules/processing/echo_canceller)
add_subdirectory(src/modules/processing/encoder)
add_subdirectory(src/modules/processing/motion_detect)
add_subdirectory(src/modules/output/audio)
add_subdirectory(src/modules/output/file)
add_subdirectory(src/modules/infra/config)
add_subdirectory(src/modules/infra/event_bus)
add_subdirectory(src/modules/infra/log)
add_subdirectory(src/modules/infra/perf_monitor)
add_subdirectory(src/modules/external_access/http)
add_subdirectory(src/modules/external_access/pusher)
add_subdirectory(src/modules/external_access/websocket)
add_subdirectory(src/runtime)
add_subdirectory(cli)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_executable(pi-guard-agent
    src/main.cpp
)

target_include_directories(pi-guard-agent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi-guard-agent
    PRIVATE
        pi_guard_runtime
)

install(TARGETS pi-guard-agent
    RUNTIME DESTINATION bin
)
