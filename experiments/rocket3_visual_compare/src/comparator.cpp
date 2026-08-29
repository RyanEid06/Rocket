#include "comparator.h"

#include <algorithm>
#include <limits>

namespace rocket3::visual_compare {
namespace {

constexpr std::size_t kChannels = 4;

Comparison failure(const char* message) {
    Comparison result;
    result.error = message;
    return result;
}

std::uint8_t channel_delta(std::uint8_t left, std::uint8_t right) {
    return left > right ? static_cast<std::uint8_t>(left - right)
                        : static_cast<std::uint8_t>(right - left);
}

} // namespace

Comparison compare(const std::vector<std::uint8_t>& expected,
                   const std::vector<std::uint8_t>& actual,
                   int width,
                   int height,
                   std::uint8_t ignored_channel_delta) {
    if (width <= 0 || height <= 0) {
        return failure("dimensions must be positive");
    }

    const auto unsigned_width = static_cast<std::size_t>(width);
    const auto unsigned_height = static_cast<std::size_t>(height);
    if (unsigned_width > std::numeric_limits<std::size_t>::max() / unsigned_height ||
        unsigned_width * unsigned_height >
            std::numeric_limits<std::size_t>::max() / kChannels) {
        return failure("RGBA buffer length does not match dimensions");
    }
    const std::size_t expected_length = unsigned_width * unsigned_height * kChannels;
    if (expected.size() != expected_length || actual.size() != expected_length) {
        return failure("RGBA buffer length does not match dimensions");
    }

    Comparison result;
    result.ok = true;
    result.metrics.difference.resize(expected_length, 0);
    result.metrics.heat.resize(expected_length, 0);

    std::size_t total_delta = 0;
    for (std::size_t offset = 0; offset < expected_length; offset += kChannels) {
        std::uint8_t pixel_max_delta = 0;
        for (std::size_t channel = 0; channel < kChannels; ++channel) {
            const std::uint8_t delta = channel_delta(expected[offset + channel], actual[offset + channel]);
            result.metrics.difference[offset + channel] = delta;
            total_delta += delta;
            pixel_max_delta = std::max(pixel_max_delta, delta);
        }
        result.metrics.max_channel_delta =
            std::max(result.metrics.max_channel_delta, pixel_max_delta);

        const bool changed = pixel_max_delta > ignored_channel_delta;
        if (!changed) {
            continue;
        }
        const std::size_t pixel = offset / kChannels;
        const int x = static_cast<int>(pixel % unsigned_width);
        const int y = static_cast<int>(pixel / unsigned_width);
        ++result.metrics.changed_pixels;
        if (!result.metrics.changed_bounds.has_pixels) {
            result.metrics.changed_bounds = Bounds{x, y, x, y, true};
        } else {
            result.metrics.changed_bounds.min_x = std::min(result.metrics.changed_bounds.min_x, x);
            result.metrics.changed_bounds.min_y = std::min(result.metrics.changed_bounds.min_y, y);
            result.metrics.changed_bounds.max_x = std::max(result.metrics.changed_bounds.max_x, x);
            result.metrics.changed_bounds.max_y = std::max(result.metrics.changed_bounds.max_y, y);
        }
        result.metrics.heat[offset] = pixel_max_delta;
        result.metrics.heat[offset + 1] = pixel_max_delta;
        result.metrics.heat[offset + 2] = pixel_max_delta;
        result.metrics.heat[offset + 3] = 255;
    }

    const double channel_count = static_cast<double>(expected_length);
    const double pixel_count = static_cast<double>(unsigned_width * unsigned_height);
    result.metrics.mean_absolute_error = static_cast<double>(total_delta) / channel_count;
    result.metrics.changed_pixel_ratio =
        static_cast<double>(result.metrics.changed_pixels) / pixel_count;
    return result;
}

} // namespace rocket3::visual_compare
