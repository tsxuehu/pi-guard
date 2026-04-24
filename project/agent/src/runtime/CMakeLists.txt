add_library(pi_guard_runtime STATIC
    app/agent_app.cpp
)

target_include_directories(pi_guard_runtime
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/public
)

target_link_libraries(pi_guard_runtime
    PUBLIC
        pi_guard_foundation
        pi_guard_capture
        pi_guard_processing
        pi_guard_output
        pi_guard_infra
        pi_guard_external_access
)
