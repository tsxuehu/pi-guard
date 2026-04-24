#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace piguard::foundation {

enum class EventType {
    MotionStart,
    MotionStop,
    RecordingStart,
    RecordingStop,
    ConnectionChange,
    ControlCommand,
};

struct VideoFrame {
    std::int64_t pts_ms{0};
    int width{0};
    int height{0};
    std::vector<std::uint8_t> data;
};

struct AudioFrame {
    std::int64_t pts_ms{0};
    int sample_rate{16000};
    int channels{1};
    std::vector<std::int16_t> samples;
};

struct Event {
    EventType type{EventType::ConnectionChange};
    std::int64_t timestamp_ms{0};
    std::string payload;
};

}  // namespace piguard::foundation
