#include "frame_converter.hpp"

#include <algorithm>

#include <opencv2/core/mat.hpp>

namespace {

uint8_t clamp_to_u8(int value) {
    return static_cast<uint8_t>(std::clamp(value, 0, 255));
}

}  // namespace

cv::Mat yuyv_to_bgr(const uint8_t* yuyv, int width, int height) {
    cv::Mat bgr(height, width, CV_8UC3);
    int out_idx = 0;

    for (int i = 0; i < width * height * 2; i += 4) {
        const int y0 = yuyv[i + 0];
        const int u = yuyv[i + 1] - 128;
        const int y1 = yuyv[i + 2];
        const int v = yuyv[i + 3] - 128;

        const int c0 = y0 - 16;
        const int c1 = y1 - 16;

        const int r0 = (298 * c0 + 409 * v + 128) >> 8;
        const int g0 = (298 * c0 - 100 * u - 208 * v + 128) >> 8;
        const int b0 = (298 * c0 + 516 * u + 128) >> 8;

        const int r1 = (298 * c1 + 409 * v + 128) >> 8;
        const int g1 = (298 * c1 - 100 * u - 208 * v + 128) >> 8;
        const int b1 = (298 * c1 + 516 * u + 128) >> 8;

        bgr.data[out_idx + 0] = clamp_to_u8(b0);
        bgr.data[out_idx + 1] = clamp_to_u8(g0);
        bgr.data[out_idx + 2] = clamp_to_u8(r0);
        bgr.data[out_idx + 3] = clamp_to_u8(b1);
        bgr.data[out_idx + 4] = clamp_to_u8(g1);
        bgr.data[out_idx + 5] = clamp_to_u8(r1);
        out_idx += 6;
    }

    return bgr;
}
