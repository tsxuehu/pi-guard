add_library(pi_guard_rtmp_pusher STATIC
    rtmp_pusher.cpp
)

target_include_directories(pi_guard_rtmp_pusher
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_rtmp_pusher
    PUBLIC
        pi_guard_foundation
)
