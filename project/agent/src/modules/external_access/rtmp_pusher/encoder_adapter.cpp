#include "rtmp_pusher/encoder_adapter.hpp"

namespace piguard::external_access {

EncoderAdapter::EncoderAdapter(piguard::processing_encoder::Encoder& encoder)
    : encoder_(encoder) {}

void EncoderAdapter::register_consumer() {
    consumer_id_ = encoder_.register_consumer();
}

void EncoderAdapter::unregister_consumer() {
    if (consumer_id_ != 0) {
        encoder_.unregister_consumer(consumer_id_);
        consumer_id_ = 0;
    }
}

std::vector<std::shared_ptr<piguard::processing_encoder::EncodedPacketBase>> EncoderAdapter::wait_packet(
    uint64_t last_seq) {
    return encoder_.wait_packet(consumer_id_, last_seq);
}

piguard::processing_encoder::EncodedVideoStreamMeta EncoderAdapter::video_stream_meta() const {
    return encoder_.video_stream_meta();
}

piguard::processing_encoder::EncodedAudioStreamMeta EncoderAdapter::audio_stream_meta() const {
    return encoder_.audio_stream_meta();
}

} // namespace piguard::external_access
