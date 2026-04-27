find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)

add_executable(preview-video
    view_video.cpp
    v4l2_capture_session.cpp
    frame_converter.cpp
    frame_viewer_consumer.cpp
)

target_link_libraries(preview-video
    PRIVATE
        pi_guard_video_capture
        ${OpenCV_LIBS}
)
