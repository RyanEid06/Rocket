#pragma once

#include <cstddef>
#include <cstdint>
#include <array>
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
    std::array<double, 4> mean_absolute_error_by_channel{};
    std::array<std::uint8_t, 4> max_delta_by_channel{};
    Bounds changed_bounds;
    std::vector<std::uint8_t> difference;
    std::vector<std::uint8_t> heat;
};

struct Tolerance {
    std::uint8_t maximum_channel_delta = 0;
    double maximum_mean_absolute_error = 0.0;
    double maximum_changed_pixel_ratio = 0.0;
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

bool within_tolerance(const Comparison& comparison,
                      const Tolerance& tolerance);

} // namespace rocket3::visual_compare
