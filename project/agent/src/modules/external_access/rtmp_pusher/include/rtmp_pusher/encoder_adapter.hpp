#pragma once

#include "rtmp_pusher/encoded_packet_adapter.hpp"
#include "processing_encoder/encoder.hpp"

namespace piguard::external_access {

class EncoderAdapter : public IEncodedPacketAdapter {
public:
    explicit EncoderAdapter(piguard::processing_encoder::Encoder& encoder);
    ~EncoderAdapter() override = default;

    EncoderAdapter(const EncoderAdapter&) = delete;
    EncoderAdapter& operator=(const EncoderAdapter&) = delete;

    void register_consumer() override;
    void unregister_consumer() override;
    std::vector<std::shared_ptr<piguard::processing_encoder::EncodedPacketBase>> wait_packet(
        uint64_t last_seq) override;
    piguard::processing_encoder::EncodedVideoStreamMeta video_stream_meta() const override;
    piguard::processing_encoder::EncodedAudioStreamMeta audio_stream_meta() const override;

private:
    piguard::processing_encoder::Encoder& encoder_;
    uint64_t consumer_id_{0};
};

} // namespace piguard::external_access
