add_library(pi_guard_encoder STATIC
    encoder.cpp
)

target_include_directories(pi_guard_encoder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_encoder
    PUBLIC
        pi_guard_foundation
)
