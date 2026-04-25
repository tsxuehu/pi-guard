add_library(pi_guard_config_manager STATIC
    config_manager.cpp
)

target_include_directories(pi_guard_config_manager
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_config_manager
    PUBLIC
        pi_guard_foundation
)
