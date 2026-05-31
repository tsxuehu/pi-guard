find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

add_library(pi_guard_rtmp_pusher STATIC
    rtmp_publisher_module.cpp
    encoder_adapter.cpp
    rtmp_pusher.cpp
)

target_include_directories(pi_guard_rtmp_pusher
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_rtmp_pusher
    PUBLIC
        pi_guard_foundation
        pi_guard_encoder
        pi_guard_infra_log
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
)
