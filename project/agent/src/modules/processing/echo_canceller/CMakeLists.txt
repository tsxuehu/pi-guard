add_library(pi_guard_echo_canceller STATIC
    echo_canceller.cpp
)

target_include_directories(pi_guard_echo_canceller
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_echo_canceller
    PUBLIC
        pi_guard_foundation
)
