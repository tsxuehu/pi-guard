add_library(pi_guard_external_access STATIC
    http/http_notify.cpp
    pusher/rtmp_pusher.cpp
    websocket/websocket_client.cpp
)

target_include_directories(pi_guard_external_access
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_external_access
    PUBLIC
        pi_guard_foundation
)
