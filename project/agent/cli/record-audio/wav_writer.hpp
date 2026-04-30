#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>

/**
 * PCM S16 LE（交织）；先写占位 WAV 头，`finalize()` 或析构时回填长度。
 */
class WavWriter {
public:
    WavWriter() = default;
    WavWriter(const WavWriter&) = delete;
    WavWriter& operator=(const WavWriter&) = delete;
    WavWriter(WavWriter&& other) noexcept { steal(std::move(other)); }
    WavWriter& operator=(WavWriter&& other) noexcept {
        if (this != &other) {
            finalize();
            steal(std::move(other));
        }
        return *this;
    }
    ~WavWriter() { finalize(); }

    bool open(const std::filesystem::path& path, unsigned channels, unsigned sample_rate_hz);

    void write_pcm_s16le(const int16_t* interleaved_samples, std::size_t sample_count);

    void finalize() noexcept;

    bool is_open() const { return ofs_.is_open(); }

private:
    static constexpr std::streamoff kHeaderBytes = 44;

    static void put_le32(char* dst, uint32_t v);
    static void put_le16(char* dst, uint16_t v);

    void write_placeholder_header();
    void steal(WavWriter&& other) noexcept;

    std::ofstream ofs_;
    uint64_t data_bytes_{0};
    unsigned channels_{1};
    unsigned sample_rate_{16000};
    bool finalized_{true};
};
