add_library(pi_guard_foundation STATIC
    shutdown_manager.cpp
)

target_include_directories(pi_guard_foundation
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)
