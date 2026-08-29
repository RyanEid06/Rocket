#include "comparator.h"

#include <cstdint>
#include <iostream>
#include <vector>

using rocket3::visual_compare::Bounds;
using rocket3::visual_compare::Comparison;
#define CHECK(condition)                                                                            \
    do {                                                                                            \
        if (!(condition)) {                                                                         \
            std::cerr << "check failed: " #condition << '\n';                                    \
            return false;                                                                           \
        }                                                                                           \
    } while (false)

static bool test_identical_buffers_are_clean() {
    const std::vector<std::uint8_t> pixels = {10, 20, 30, 255, 40, 50, 60, 255};
    const Comparison result = rocket3::visual_compare::compare(pixels, pixels, 2, 1, 0);
    CHECK(result.ok);
    CHECK(result.metrics.changed_pixels == 0);
    CHECK(result.metrics.mean_absolute_error == 0.0);
    CHECK(result.metrics.changed_pixel_ratio == 0.0);
    CHECK(result.metrics.max_channel_delta == 0);
    CHECK(!result.metrics.changed_bounds.has_pixels);
    CHECK(result.metrics.difference == std::vector<std::uint8_t>(8, 0));
    CHECK(result.metrics.heat == std::vector<std::uint8_t>(8, 0));
    return true;
}

static bool test_threshold_metrics_artifacts_and_bounds() {
    const std::vector<std::uint8_t> expected = {
        10, 20, 30, 255,
        100, 100, 100, 255,
        0, 0, 0, 255,
        0, 0, 0, 255,
    };
    const std::vector<std::uint8_t> actual = {
        12, 20, 30, 255,
        110, 90, 100, 255,
        0, 0, 0, 255,
        0, 0, 5, 255,
    };
    const Comparison result = rocket3::visual_compare::compare(expected, actual, 2, 2, 3);
    CHECK(result.ok);
    CHECK(result.metrics.changed_pixels == 2);
    CHECK(result.metrics.mean_absolute_error == 1.6875);
    CHECK(result.metrics.changed_pixel_ratio == 0.5);
    CHECK(result.metrics.max_channel_delta == 10);
    const Bounds bounds = result.metrics.changed_bounds;
    CHECK(bounds.has_pixels);
    CHECK(bounds.min_x == 1);
    CHECK(bounds.min_y == 0);
    CHECK(bounds.max_x == 1);
    CHECK(bounds.max_y == 1);
    CHECK(result.metrics.difference == std::vector<std::uint8_t>({2, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0}));
    CHECK(result.metrics.heat == std::vector<std::uint8_t>({0, 0, 0, 0, 10, 10, 10, 255, 0, 0, 0, 0, 5, 5, 5, 255}));
    return true;
}

static bool test_invalid_inputs_are_explicit_and_deterministic() {
    const std::vector<std::uint8_t> pixels(4, 0);
    const Comparison bad_dimensions = rocket3::visual_compare::compare(pixels, pixels, 0, 1, 0);
    CHECK(!bad_dimensions.ok);
    CHECK(bad_dimensions.error == "dimensions must be positive");

    const Comparison bad_length = rocket3::visual_compare::compare(pixels, pixels, 2, 2, 0);
    CHECK(!bad_length.ok);
    CHECK(bad_length.error == "RGBA buffer length does not match dimensions");

    const Comparison mismatched = rocket3::visual_compare::compare(
        std::vector<std::uint8_t>(8, 0), std::vector<std::uint8_t>(4, 0), 1, 2, 0);
    CHECK(!mismatched.ok);
    CHECK(mismatched.error == "RGBA buffer length does not match dimensions");
    return true;
}

int main() {
    return test_identical_buffers_are_clean() &&
                   test_threshold_metrics_artifacts_and_bounds() &&
                   test_invalid_inputs_are_explicit_and_deterministic()
               ? 0
               : 1;
}
