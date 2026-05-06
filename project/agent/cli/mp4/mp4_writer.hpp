#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "processing_encoder/encoder_types.hpp"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace piguard {

class Mp4Writer {
public:
    Mp4Writer(const std::string& output_path,
              const processing_encoder::EncodedVideoStreamMeta& vmeta,
              const processing_encoder::EncodedAudioStreamMeta& ameta);
    ~Mp4Writer();

    Mp4Writer(const Mp4Writer&) = delete;
    Mp4Writer& operator=(const Mp4Writer&) = delete;

    bool write_header();
    bool write_packet(std::shared_ptr<processing_encoder::EncodedPacketBase> packet);
    bool write_trailer();

private:
    AVFormatContext* fmt_ctx_{nullptr};
    AVStream* vstream_{nullptr};
    AVStream* astream_{nullptr};
    processing_encoder::EncodedVideoStreamMeta vmeta_;
    processing_encoder::EncodedAudioStreamMeta ameta_;
};

}  // namespace piguard
