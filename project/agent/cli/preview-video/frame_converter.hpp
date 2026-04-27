#pragma once

#include <cstdint>

#include <opencv2/core/mat.hpp>

cv::Mat yuyv_to_bgr(const uint8_t* yuyv, int width, int height);
