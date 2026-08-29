#include "evidence_schema.h"

#include <cmath>

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

} // namespace rocket3::visual_compare
