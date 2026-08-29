#include "comparator.h"

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

using rocket3::visual_compare::Bounds;
using rocket3::visual_compare::Comparison;
using rocket3::visual_compare::Metrics;

static void test_identical_buffers_are_clean() {
    const std::vector<std::uint8_t> pixels = {10, 20, 30, 255, 40, 50, 60, 255};
    const Comparison result = rocket3::visual_compare::compare(pixels, pixels, 2, 1, 0);
    assert(result.ok);
    assert(result.metrics.changed_pixels == 0);
    assert(result.metrics.mean_absolute_error == 0.0);
    assert(result.metrics.changed_pixel_ratio == 0.0);
    assert(result.metrics.max_channel_delta == 0);
    assert(!result.metrics.changed_bounds.has_pixels);
    assert(result.metrics.difference == std::vector<std::uint8_t>(8, 0));
    assert(result.metrics.heat == std::vector<std::uint8_t>(8, 0));
}

static void test_threshold_metrics_artifacts_and_bounds() {
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
    assert(result.ok);
    assert(result.metrics.changed_pixels == 2);
    assert(result.metrics.mean_absolute_error == 1.6875);
    assert(result.metrics.changed_pixel_ratio == 0.5);
    assert(result.metrics.max_channel_delta == 10);
    const Bounds bounds = result.metrics.changed_bounds;
    assert(bounds.has_pixels);
    assert(bounds.min_x == 1);
    assert(bounds.min_y == 0);
    assert(bounds.max_x == 1);
    assert(bounds.max_y == 1);
    assert(result.metrics.difference == std::vector<std::uint8_t>({2, 0, 0, 0, 10, 10, 0, 0, 0, 0, 0, 0, 0, 0, 5, 0}));
    assert(result.metrics.heat == std::vector<std::uint8_t>({0, 0, 0, 0, 10, 10, 10, 255, 0, 0, 0, 0, 5, 5, 5, 255}));
}

static void test_invalid_inputs_are_explicit_and_deterministic() {
    const std::vector<std::uint8_t> pixels(4, 0);
    const Comparison bad_dimensions = rocket3::visual_compare::compare(pixels, pixels, 0, 1, 0);
    assert(!bad_dimensions.ok);
    assert(bad_dimensions.error == "dimensions must be positive");

    const Comparison bad_length = rocket3::visual_compare::compare(pixels, pixels, 2, 2, 0);
    assert(!bad_length.ok);
    assert(bad_length.error == "RGBA buffer length does not match dimensions");

    const Comparison mismatched = rocket3::visual_compare::compare(
        std::vector<std::uint8_t>(8, 0), std::vector<std::uint8_t>(4, 0), 1, 2, 0);
    assert(!mismatched.ok);
    assert(mismatched.error == "RGBA buffer length does not match dimensions");
}

int main() {
    test_identical_buffers_are_clean();
    test_threshold_metrics_artifacts_and_bounds();
    test_invalid_inputs_are_explicit_and_deterministic();
    return 0;
}
