#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include "processing_encoder/encoder_types.hpp"

namespace piguard::external_access {

class IEncodedPacketAdapter {
public:
    virtual ~IEncodedPacketAdapter() = default;

    virtual void register_consumer() = 0;
    virtual void unregister_consumer() = 0;
    virtual std::vector<std::shared_ptr<piguard::processing_encoder::EncodedPacketBase>> wait_packet(
        uint64_t last_seq) = 0;
    virtual piguard::processing_encoder::EncodedVideoStreamMeta video_stream_meta() const = 0;
    virtual piguard::processing_encoder::EncodedAudioStreamMeta audio_stream_meta() const = 0;
};

} // namespace piguard::external_access
