#pragma once

#include <cstddef>
#include <cstdint>

namespace piguard::cli::rtmp_pusher {

struct Config {
    int video_width = 640;
    int video_height = 480;
    int video_fps = 25;
    uint32_t video_buffer_count = 4;
    size_t provider_queue_capacity = 50;
    unsigned audio_sample_rate = 16000;
    unsigned audio_channels = 1;
    
    const char* default_video_device = "/dev/video0";
    const char* default_audio_device = "default";
    const char* default_rtmp_url = "rtmp://127.0.0.1/live/livestream";
};

} // namespace piguard::cli::rtmp_pusher
