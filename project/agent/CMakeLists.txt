cmake_minimum_required(VERSION 3.16)

project(pi_guard_agent VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_subdirectory(src/foundation)
add_subdirectory(src/modules/capture)
add_subdirectory(src/modules/processing)
add_subdirectory(src/modules/output)
add_subdirectory(src/modules/infra)
add_subdirectory(src/modules/external_access)
add_subdirectory(src/runtime)

include(CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

add_executable(pi-guard-agent
    src/main.cpp
)

target_include_directories(pi-guard-agent
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi-guard-agent
    PRIVATE
        pi_guard_runtime
)

install(TARGETS pi-guard-agent
    RUNTIME DESTINATION bin
)
