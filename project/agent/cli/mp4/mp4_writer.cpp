#include "mp4_writer.hpp"

#include <cstring>

#include <libavutil/mem.h>

namespace piguard {

Mp4Writer::Mp4Writer(const std::string& output_path,
                     const processing_encoder::EncodedVideoStreamMeta& vmeta,
                     const processing_encoder::EncodedAudioStreamMeta& ameta)
    : vmeta_(vmeta), ameta_(ameta) {
    avformat_alloc_output_context2(&fmt_ctx_, nullptr, "mp4", output_path.c_str());
}

Mp4Writer::~Mp4Writer() {
    if (fmt_ctx_ != nullptr) {
        avformat_free_context(fmt_ctx_);
    }
}

bool Mp4Writer::write_header() {
    if (fmt_ctx_ == nullptr) {
        return false;
    }

    vstream_ = avformat_new_stream(fmt_ctx_, nullptr);
    astream_ = avformat_new_stream(fmt_ctx_, nullptr);
    if (vstream_ == nullptr || astream_ == nullptr) {
        return false;
    }

    vstream_->time_base = AVRational{vmeta_.time_base_num, vmeta_.time_base_den};
    vstream_->codecpar->codec_type = AVMEDIA_TYPE_VIDEO;
    vstream_->codecpar->codec_id = static_cast<AVCodecID>(vmeta_.codec_id);
    vstream_->codecpar->width = vmeta_.width;
    vstream_->codecpar->height = vmeta_.height;
    if (!vmeta_.extradata.empty()) {
        vstream_->codecpar->extradata_size = static_cast<int>(vmeta_.extradata.size());
        vstream_->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(vmeta_.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(vstream_->codecpar->extradata, vmeta_.extradata.data(), vmeta_.extradata.size());
    }

    astream_->time_base = AVRational{ameta_.time_base_num, ameta_.time_base_den};
    astream_->codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    astream_->codecpar->codec_id = static_cast<AVCodecID>(ameta_.codec_id);
    astream_->codecpar->sample_rate = ameta_.sample_rate;
    av_channel_layout_default(&astream_->codecpar->ch_layout, ameta_.channels);
    if (!ameta_.extradata.empty()) {
        astream_->codecpar->extradata_size = static_cast<int>(ameta_.extradata.size());
        astream_->codecpar->extradata = static_cast<uint8_t*>(
            av_mallocz(ameta_.extradata.size() + AV_INPUT_BUFFER_PADDING_SIZE));
        std::memcpy(astream_->codecpar->extradata, ameta_.extradata.data(), ameta_.extradata.size());
    }

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&fmt_ctx_->pb, fmt_ctx_->url, AVIO_FLAG_WRITE) < 0) {
            return false;
        }
    }

    if (avformat_write_header(fmt_ctx_, nullptr) < 0) {
        return false;
    }

    return true;
}

bool Mp4Writer::write_packet(std::shared_ptr<processing_encoder::EncodedPacketBase> packet) {
    if (fmt_ctx_ == nullptr) {
        return false;
    }

    AVPacket pkt{};
    av_init_packet(&pkt);
    pkt.data = const_cast<uint8_t*>(packet->data.data());
    pkt.size = static_cast<int>(packet->data.size());
    pkt.pts = packet->pts;
    pkt.dts = packet->dts;

    if (auto video_pkt = std::dynamic_pointer_cast<processing_encoder::EncodedVideoPacket>(packet)) {
        if (video_pkt->key_frame) {
            pkt.flags |= AV_PKT_FLAG_KEY;
        }
        pkt.stream_index = vstream_->index;
        av_packet_rescale_ts(&pkt,
                             AVRational{vmeta_.time_base_num, vmeta_.time_base_den},
                             vstream_->time_base);
    } else {
        pkt.flags |= AV_PKT_FLAG_KEY;
        pkt.stream_index = astream_->index;
        av_packet_rescale_ts(&pkt,
                             AVRational{ameta_.time_base_num, ameta_.time_base_den},
                             astream_->time_base);
    }

    if (av_interleaved_write_frame(fmt_ctx_, &pkt) < 0) {
        return false;
    }

    return true;
}

bool Mp4Writer::write_trailer() {
    if (fmt_ctx_ == nullptr) {
        return false;
    }

    av_write_trailer(fmt_ctx_);

    if (!(fmt_ctx_->oformat->flags & AVFMT_NOFILE)) {
        avio_closep(&fmt_ctx_->pb);
    }

    return true;
}

}  // namespace piguard
