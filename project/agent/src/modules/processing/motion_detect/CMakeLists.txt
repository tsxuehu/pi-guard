add_library(pi_guard_motion_detect STATIC
    motion_detect.cpp
)

target_include_directories(pi_guard_motion_detect
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_motion_detect
    PUBLIC
        pi_guard_foundation
)
