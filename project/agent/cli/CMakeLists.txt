find_package(OpenCV REQUIRED COMPONENTS core highgui imgproc)

add_executable(view_video
    view_video.cpp
)

target_link_libraries(view_video
    PRIVATE
        pi_guard_video_capture
        ${OpenCV_LIBS}
)
