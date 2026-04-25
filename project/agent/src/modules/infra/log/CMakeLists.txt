add_library(pi_guard_logger STATIC
    logger.cpp
)

target_include_directories(pi_guard_logger
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_logger
    PUBLIC
        pi_guard_foundation
)
