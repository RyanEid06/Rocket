#include "evidence_schema.h"

#include <cmath>
#include <iostream>
#include <string>

using rocket3::visual_compare::GoldenApproval;
using rocket3::visual_compare::MeasuredThreshold;

#define CHECK(condition) \
    do { \
        if (!(condition)) { \
            std::cerr << "check failed: " #condition << '\\n'; \
            return false; \
        } \
    } while (false)

static bool test_fixture_has_versioned_deterministic_identity_and_environment() {
    const auto first = rocket3::visual_compare::make_synthetic_fixture();
    const auto second = rocket3::visual_compare::make_synthetic_fixture();
    CHECK(first.schema.major == 1);
    CHECK(first.schema.minor == 0);
    CHECK(first.scene.key() == "scene:card-table/compact/v1:1280x720");
    CHECK(first.scene.key() == second.scene.key());
    CHECK(first.metric.key() == "metric:frame_time_ms/mean/v1");
    CHECK(first.metric.key() == second.metric.key());
    CHECK(first.environment.os == "windows");
    CHECK(first.environment.arch == "x86_64");
    CHECK(first.environment.compiler == "clang");
    CHECK(first.environment.build_type == "release");
    CHECK(first.environment.logical_width == 1280);
    CHECK(first.environment.logical_height == 720);
    CHECK(std::abs(first.environment.scale_factor - 1.25) < 0.000001);
    return true;
}

static bool test_fixture_carries_measured_counters_and_thresholds() {
    const auto fixture = rocket3::visual_compare::make_synthetic_fixture();
    CHECK(fixture.counters.allocations == 12);
    CHECK(fixture.counters.cache_hits == 96);
    CHECK(fixture.counters.cache_misses == 4);
    CHECK(fixture.counters.uploads == 3);
    CHECK(fixture.counters.state_switches == 2);
    CHECK(fixture.counters.ffi_calls == 8);
    CHECK(fixture.threshold.measured == 4.5);
    CHECK(fixture.threshold.limit == 5.0);
    CHECK(fixture.threshold.inclusive);
    CHECK(fixture.threshold.passed());
    CHECK(fixture.threshold.metric.key() == fixture.metric.key());
    return true;
}

static bool test_failure_manifest_and_golden_record_are_explicit() {
    const auto fixture = rocket3::visual_compare::make_synthetic_fixture();
    CHECK(fixture.failure_artifacts.size() == 2);
    CHECK(fixture.failure_artifacts[0].kind == "diff-raw-rgba");
    CHECK(fixture.failure_artifacts[0].path == "out/rocket3-provisional/wp08/diff.rgba");
    CHECK(fixture.failure_artifacts[0].sha256 == "sha256:fixture-diff-v1");
    CHECK(fixture.failure_artifacts[1].kind == "metrics-record");
    CHECK(fixture.golden.status == GoldenApproval::Status::Proposed);
    CHECK(fixture.golden.scene_key == fixture.scene.key());
    CHECK(fixture.golden.baseline_hash == "sha256:fixture-golden-v1");
    CHECK(fixture.golden.approval_id == "wp08-fixture-approval-001");
    CHECK(!fixture.golden.update_allowed);
    return true;
}

static bool test_threshold_rejects_over_limit_without_final_budget_claim() {
    MeasuredThreshold threshold;
    threshold.measured = 5.01;
    threshold.limit = 5.0;
    threshold.inclusive = true;
    CHECK(!threshold.passed());
    threshold.inclusive = false;
    threshold.measured = 5.0;
    CHECK(!threshold.passed());
    return true;
}

int main() {
    return test_fixture_has_versioned_deterministic_identity_and_environment() &&
                   test_fixture_carries_measured_counters_and_thresholds() &&
                   test_failure_manifest_and_golden_record_are_explicit() &&
                   test_threshold_rejects_over_limit_without_final_budget_claim()
               ? 0
               : 1;
}
