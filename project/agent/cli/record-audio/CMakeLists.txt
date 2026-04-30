add_executable(record-audio
    main.cpp
    wav_writer.cpp
    recording_consumer.cpp
)

target_include_directories(record-audio
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(record-audio
    PRIVATE
        pi_guard_audio_capture
        pi_guard_infra_log
)
