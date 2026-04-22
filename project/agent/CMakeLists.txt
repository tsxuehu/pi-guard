cmake_minimum_required(VERSION 3.16)

project(pi_guard_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(pi-guard-agent
    src/main.cpp
)

target_include_directories(pi-guard-agent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

install(TARGETS pi-guard-agent
    RUNTIME DESTINATION bin
)
