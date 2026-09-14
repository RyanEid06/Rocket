#include "evidence_schema.h"

#include <cmath>
#include <limits>

namespace rocket3::visual_compare {

std::string SceneIdentity::key() const {
    return "scene:" + name + "/" + variant + ":" + std::to_string(width) + "x" +
           std::to_string(height);
}

std::string MetricIdentity::key() const {
    return "metric:" + name + "/" + aggregation + "/v1";
}

bool MeasuredThreshold::passed() const {
    if (!std::isfinite(measured) || !std::isfinite(limit)) {
        return false;
    }
    return inclusive ? measured <= limit : measured < limit;
}

EvidenceFixture make_synthetic_fixture() {
    EvidenceFixture fixture;
    fixture.scene = {"card-table", "compact/v1", 1280, 720};
    fixture.metric = {"frame_time_ms", "mean"};
    fixture.environment = {"windows", "x86_64", "clang", "release", 1280, 720, 1.25};
    fixture.counters = {12, 96, 4, 3, 2, 8};
    fixture.threshold = {fixture.metric, 4.5, 5.0, true};
    fixture.failure_artifacts = {
        {"diff-raw-rgba", "out/rocket3-provisional/wp08/diff.rgba", "sha256:fixture-diff-v1"},
        {"metrics-record", "out/rocket3-provisional/wp08/metrics.txt", "sha256:fixture-metrics-v1"},
    };
    fixture.golden = {GoldenApproval::Status::Proposed,
                      "wp08-fixture-approval-001",
                      fixture.scene.key(),
                      "sha256:fixture-golden-v1",
                      "",
                      "",
                      false};
    return fixture;
}

PerformanceBudget final_performance_budget() {
    PerformanceBudget budget;
    budget.warmup_frames = 30;
    budget.measured_frames = 120;
    budget.maximum_native_allocations_per_frame = 1;
    budget.maximum_temporary_strings = 0;
    budget.maximum_layout_allocations_per_frame = 1;
    budget.maximum_layout_recomputations = 0;
    budget.maximum_text_measurements_per_frame = 2;
    budget.maximum_asset_lookups_per_frame = 1;
    budget.maximum_ffi_calls_per_frame = 12;
    budget.maximum_render_target_switches_per_frame = 2;
    budget.maximum_texture_uploads = 0;
    budget.maximum_peak_state_growth = 1;
    budget.maximum_peak_cache_growth = 0;
    budget.maximum_mean_frame_time_us = 100.0;
    budget.maximum_frame_time_us = 2000.0;
    return budget;
}

namespace {

bool exceeds_per_frame(std::uint64_t total, std::uint64_t frames,
                       std::uint64_t maximum) {
    if (frames == 0) return true;
    if (maximum > std::numeric_limits<std::uint64_t>::max() / frames) {
        return false;
    }
    return total > maximum * frames;
}

} // namespace

std::vector<std::string> performance_budget_violations(
    const PerformanceObservation& observation,
    const PerformanceBudget& budget) {
    std::vector<std::string> violations;
    if (observation.measured_frames != budget.measured_frames) {
        violations.push_back("measured frame count does not match final budget");
    }
    const std::uint64_t frames = observation.measured_frames;
    const auto check_per_frame = [&](std::uint64_t value,
                                     std::uint64_t maximum,
                                     const char* name) {
        if (exceeds_per_frame(value, frames, maximum)) {
            violations.push_back(std::string(name) + " exceeded per-frame budget");
        }
    };
    const auto check_total = [&](std::uint64_t value,
                                  std::uint64_t maximum,
                                  const char* name) {
        if (value > maximum) {
            violations.push_back(std::string(name) + " exceeded steady-state budget");
        }
    };
    check_per_frame(observation.native_allocations,
                    budget.maximum_native_allocations_per_frame,
                    "native allocations");
    check_total(observation.temporary_strings,
                budget.maximum_temporary_strings, "temporary strings");
    check_per_frame(observation.layout_allocations,
                    budget.maximum_layout_allocations_per_frame,
                    "layout allocations");
    check_total(observation.layout_recomputations,
                budget.maximum_layout_recomputations,
                "layout recomputations");
    check_per_frame(observation.text_measurements,
                    budget.maximum_text_measurements_per_frame,
                    "text measurements");
    check_per_frame(observation.asset_lookups,
                    budget.maximum_asset_lookups_per_frame,
                    "asset lookups");
    check_per_frame(observation.ffi_calls,
                    budget.maximum_ffi_calls_per_frame, "FFI calls");
    check_per_frame(observation.render_target_switches,
                    budget.maximum_render_target_switches_per_frame,
                    "render-target switches");
    check_total(observation.texture_uploads,
                budget.maximum_texture_uploads, "texture uploads");
    check_total(observation.peak_state_growth,
                budget.maximum_peak_state_growth, "state growth");
    check_total(observation.peak_cache_growth,
                budget.maximum_peak_cache_growth, "cache growth");
    if (!std::isfinite(observation.mean_frame_time_us) ||
        observation.mean_frame_time_us < 0.0 ||
        observation.mean_frame_time_us > budget.maximum_mean_frame_time_us) {
        violations.push_back("mean frame time exceeded final budget");
    }
    if (!std::isfinite(observation.maximum_frame_time_us) ||
        observation.maximum_frame_time_us < 0.0 ||
        observation.maximum_frame_time_us > budget.maximum_frame_time_us) {
        violations.push_back("maximum frame time exceeded final budget");
    }
    return violations;
}

} // namespace rocket3::visual_compare
