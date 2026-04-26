add_library(pi_guard_infra_log STATIC
    logger_factory.cpp
    log_module.cpp
    spd_logger.cpp
)

set(PI_GUARD_LOG_BACKEND "spdlog" CACHE STRING "Log backend: currently only spdlog")
set_property(CACHE PI_GUARD_LOG_BACKEND PROPERTY STRINGS spdlog)

target_include_directories(pi_guard_infra_log
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_infra_log
    PUBLIC
        pi_guard_foundation
)

if(NOT PI_GUARD_LOG_BACKEND STREQUAL "spdlog")
    message(FATAL_ERROR "Unsupported PI_GUARD_LOG_BACKEND: ${PI_GUARD_LOG_BACKEND}")
endif()

find_package(spdlog REQUIRED)
target_link_libraries(pi_guard_infra_log PUBLIC spdlog::spdlog)
