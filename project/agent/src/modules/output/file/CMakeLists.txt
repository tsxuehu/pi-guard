add_library(pi_guard_output_file STATIC
    file_writer.cpp
)

target_include_directories(pi_guard_output_file
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_output_file
    PUBLIC
        pi_guard_foundation
)
