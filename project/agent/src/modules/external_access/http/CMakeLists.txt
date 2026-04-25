add_library(pi_guard_http_notify STATIC
    http_notify.cpp
)

target_include_directories(pi_guard_http_notify
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_http_notify
    PUBLIC
        pi_guard_foundation
)
