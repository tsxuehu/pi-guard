#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "capture_audio/audio_frame.hpp"
#include "capture_video/video_frame.hpp"

namespace piguard::processing_encoder {

class IVideoFrameGetter {
public:
    virtual ~IVideoFrameGetter() = default;
    virtual std::vector<std::shared_ptr<piguard::capture_video::VideoFrame>> fetch_frames() = 0;
};

class IAudioFrameGetter {
public:
    virtual ~IAudioFrameGetter() = default;
    virtual std::vector<std::shared_ptr<piguard::capture_audio::AudioFrame>> fetch_frames() = 0;
};

struct EncoderOptions {
    int video_width{640};
    int video_height{480};
    int video_fps{25};
    int video_bitrate{1'500'000};
    int audio_sample_rate{16'000};
    int audio_channels{1};
    int audio_bitrate{64'000};
    size_t packet_queue_capacity{300};
};

enum class EncodedStreamType {
    kVideo,
    kAudio
};

struct EncodedPacketBase {
    uint64_t seq{0};
    std::vector<uint8_t> data;
    int64_t pts{0};
    int64_t dts{0};
    virtual ~EncodedPacketBase() = default;
};

struct EncodedVideoPacket : public EncodedPacketBase {
    bool key_frame{false};
};

struct EncodedAudioPacket : public EncodedPacketBase {};

struct EncodedStreamMetaBase {
    bool ready{false};
    int codec_id{0};
    int time_base_num{1};
    int time_base_den{1};
    std::vector<uint8_t> extradata;
    virtual ~EncodedStreamMetaBase() = default;
};

struct EncodedVideoStreamMeta : public EncodedStreamMetaBase {
    int width{0};
    int height{0};
};

struct EncodedAudioStreamMeta : public EncodedStreamMetaBase {
    int sample_rate{0};
    int channels{0};
};

}  // namespace piguard::processing_encoder
