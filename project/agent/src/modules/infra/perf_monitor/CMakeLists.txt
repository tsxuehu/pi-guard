add_library(pi_guard_perf_monitor STATIC
    perf_monitor.cpp
)

target_include_directories(pi_guard_perf_monitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_perf_monitor
    PUBLIC
        pi_guard_foundation
)
