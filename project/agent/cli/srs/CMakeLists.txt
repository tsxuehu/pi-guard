find_package(PkgConfig REQUIRED)
pkg_check_modules(AVFORMAT REQUIRED IMPORTED_TARGET libavformat)
pkg_check_modules(AVCODEC REQUIRED IMPORTED_TARGET libavcodec)
pkg_check_modules(AVUTIL REQUIRED IMPORTED_TARGET libavutil)

add_executable(srs-push-demo
    main.cpp
)

target_link_libraries(srs-push-demo
    PRIVATE
        pi_guard_foundation
        pi_guard_infra_log
        pi_guard_video_capture
        pi_guard_audio_capture
        pi_guard_encoder
        PkgConfig::AVFORMAT
        PkgConfig::AVCODEC
        PkgConfig::AVUTIL
)
