#pragma once

#include <cstdint>
#include <vector>

/**
 * @brief 一包 PCM 采集数据（与消费者、分发队列共用）。
 */
namespace piguard::capture_audio {

struct AudioFrame {
    uint64_t seq;
    std::vector<int16_t> pcm_data; // PCM S16_LE 交织数据
    uint64_t timestamp;            // 采集时刻（steady_clock epoch，实现侧约定单位）
};

}  // namespace piguard::capture_audio
