#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rocket3::visual_compare {

struct Bounds {
    int min_x = 0;
    int min_y = 0;
    int max_x = 0;
    int max_y = 0;
    bool has_pixels = false;
};

struct Metrics {
    std::size_t changed_pixels = 0;
    double mean_absolute_error = 0.0;
    double changed_pixel_ratio = 0.0;
    std::uint8_t max_channel_delta = 0;
    Bounds changed_bounds;
    std::vector<std::uint8_t> difference;
    std::vector<std::uint8_t> heat;
};

struct Comparison {
    bool ok = false;
    std::string error;
    Metrics metrics;
};

Comparison compare(const std::vector<std::uint8_t>& expected,
                   const std::vector<std::uint8_t>& actual,
                   int width,
                   int height,
                   std::uint8_t ignored_channel_delta);

} // namespace rocket3::visual_compare
