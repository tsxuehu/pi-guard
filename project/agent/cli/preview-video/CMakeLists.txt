find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)

add_executable(preview-video
    main.cpp
    frame_converter.cpp
    frame_viewer_consumer.cpp
)

target_link_libraries(preview-video
    PRIVATE
        pi_guard_video_capture
        pi_guard_foundation
        pi_guard_infra_log
        ${OpenCV_LIBS}
)
