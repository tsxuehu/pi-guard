add_library(pi_guard_processing STATIC
    echo_canceller/echo_canceller.cpp
    encoder/encoder.cpp
    motion_detect/motion_detect.cpp
)

target_include_directories(pi_guard_processing
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_processing
    PUBLIC
        pi_guard_foundation
)
