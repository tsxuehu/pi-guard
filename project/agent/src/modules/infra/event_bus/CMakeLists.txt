add_library(pi_guard_event_bus STATIC
    event_bus.cpp
)

target_include_directories(pi_guard_event_bus
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_event_bus
    PUBLIC
        pi_guard_foundation
)
