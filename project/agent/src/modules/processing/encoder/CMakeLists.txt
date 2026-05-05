find_package(PkgConfig REQUIRED)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)
pkg_check_modules(SWSCALE REQUIRED IMPORTED_TARGET libswscale)
pkg_check_modules(SWRESAMPLE REQUIRED IMPORTED_TARGET libswresample)

add_library(pi_guard_encoder STATIC
    encoder.cpp
    encoder_module.cpp
    video_provider_adapter.cpp
    audio_provider_adapter.cpp
)

target_include_directories(pi_guard_encoder
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(pi_guard_encoder
    PUBLIC
        pi_guard_foundation
        pi_guard_video_capture
        pi_guard_audio_capture
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
        PkgConfig::SWSCALE
        PkgConfig::SWRESAMPLE
)
