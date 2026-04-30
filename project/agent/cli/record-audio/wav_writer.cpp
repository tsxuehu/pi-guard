#include "wav_writer.hpp"

#include <cstring>

namespace {

constexpr unsigned kBitsPerSample = 16;

unsigned byte_rate_hz(unsigned channels, unsigned rate_hz) {
    return channels * rate_hz * (kBitsPerSample / 8u);
}

unsigned block_align(unsigned channels) {
    return channels * (kBitsPerSample / 8u);
}

}  // namespace

void WavWriter::put_le32(char* dst, uint32_t v) {
    dst[0] = static_cast<char>(v & 0xff);
    dst[1] = static_cast<char>((v >> 8) & 0xff);
    dst[2] = static_cast<char>((v >> 16) & 0xff);
    dst[3] = static_cast<char>((v >> 24) & 0xff);
}

void WavWriter::put_le16(char* dst, uint16_t v) {
    dst[0] = static_cast<char>(v & 0xff);
    dst[1] = static_cast<char>((v >> 8) & 0xff);
}

void WavWriter::steal(WavWriter&& other) noexcept {
    ofs_.swap(other.ofs_);
    data_bytes_ = other.data_bytes_;
    channels_ = other.channels_;
    sample_rate_ = other.sample_rate_;
    finalized_ = other.finalized_;
    other.finalized_ = true;
    other.ofs_.close();
}

bool WavWriter::open(const std::filesystem::path& path, unsigned channels, unsigned sample_rate_hz) {
    finalize();

    ofs_.open(path, std::ios::binary | std::ios::trunc | std::ios::out);
    if (!ofs_) {
        return false;
    }
    channels_ = channels;
    sample_rate_ = sample_rate_hz;
    data_bytes_ = 0;
    finalized_ = false;

    write_placeholder_header();
    return true;
}

void WavWriter::write_placeholder_header() {
    char h[static_cast<std::size_t>(kHeaderBytes)]{};
    static const char riff[] = "RIFF";
    static const char wave[] = "WAVE";
    static const char fmt[] = "fmt ";
    static const char data[] = "data";

    std::memcpy(h + 0, riff, 4);                 // riff id
    put_le32(h + 4, 36u);                          // riff chunk size placeholder
    std::memcpy(h + 8, wave, 4);
    std::memcpy(h + 12, fmt, 4);                   // fmt chunk id
    put_le32(h + 16, 16);                          // fmt chunk size pcm
    put_le16(h + 20, 1);                           // pcm format tag
    put_le16(h + 22, static_cast<uint16_t>(channels_));
    put_le32(h + 24, sample_rate_);
    put_le32(h + 28, byte_rate_hz(channels_, sample_rate_));
    put_le16(h + 32, static_cast<uint16_t>(block_align(channels_)));
    put_le16(h + 34, kBitsPerSample);
    std::memcpy(h + 36, data, 4);
    put_le32(h + 40, 0);                           // pcm data chunk size placeholder

    ofs_.write(h, sizeof(h));
}

void WavWriter::write_pcm_s16le(const int16_t* interleaved_samples, std::size_t sample_count) {
    if (!ofs_ || finalized_ || sample_count == 0 || interleaved_samples == nullptr) {
        return;
    }
    ofs_.write(reinterpret_cast<const char*>(interleaved_samples),
               static_cast<std::streamsize>(sample_count * sizeof(int16_t)));
    data_bytes_ += static_cast<uint64_t>(sample_count * sizeof(int16_t));
}

void WavWriter::finalize() noexcept {
    if (finalized_ || !ofs_.is_open()) {
        return;
    }
    ofs_.seekp(4, std::ios::beg);  // RIFF chunk size (bytes 4..7)

    uint32_t riff_chunksize =
        static_cast<uint32_t>(36u + data_bytes_);
    char le4[4];
    put_le32(le4, riff_chunksize);

    ofs_.write(le4, 4);  // field at 4: file_size - 8

    ofs_.seekp(40, std::ios::beg);  // Subchunk2Size at offset 40
    put_le32(le4, static_cast<uint32_t>(data_bytes_));
    ofs_.write(le4, 4);

    ofs_.flush();
    ofs_.close();
    finalized_ = true;
}
