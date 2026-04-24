add_library(pi_guard_infra STATIC
    config/config_manager.cpp
    event_bus/event_bus.cpp
    log/logger.cpp
    perf_monitor/perf_monitor.cpp
)

target_include_directories(pi_guard_infra
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_infra
    PUBLIC
        pi_guard_foundation
)
