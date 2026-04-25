add_library(pi_guard_websocket_client STATIC
    websocket_client.cpp
)

target_include_directories(pi_guard_websocket_client
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_websocket_client
    PUBLIC
        pi_guard_foundation
)
