#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace rocket3::visual_compare {

struct SchemaVersion {
    int major = 1;
    int minor = 0;
};

struct SceneIdentity {
    std::string name;
    std::string variant;
    int width = 0;
    int height = 0;

    std::string key() const;
};

struct MetricIdentity {
    std::string name;
    std::string aggregation;

    std::string key() const;
};

struct EnvironmentMetadata {
    std::string os;
    std::string arch;
    std::string compiler;
    std::string build_type;
    int logical_width = 0;
    int logical_height = 0;
    double scale_factor = 1.0;
};

struct Counters {
    std::uint64_t allocations = 0;
    std::uint64_t cache_hits = 0;
    std::uint64_t cache_misses = 0;
    std::uint64_t uploads = 0;
    std::uint64_t state_switches = 0;
    std::uint64_t ffi_calls = 0;
};

struct MeasuredThreshold {
    MetricIdentity metric;
    double measured = 0.0;
    double limit = 0.0;
    bool inclusive = true;

    bool passed() const;
};

struct FailureArtifact {
    std::string kind;
    std::string path;
    std::string sha256;
};

struct GoldenApproval {
    enum class Status { Proposed, Approved, Rejected };

    Status status = Status::Proposed;
    std::string approval_id;
    std::string scene_key;
    std::string baseline_hash;
    std::string approved_by;
    std::string approved_at;
    bool update_allowed = false;
};

struct EvidenceFixture {
    SchemaVersion schema;
    SceneIdentity scene;
    MetricIdentity metric;
    EnvironmentMetadata environment;
    Counters counters;
    MeasuredThreshold threshold;
    std::vector<FailureArtifact> failure_artifacts;
    GoldenApproval golden;
};

EvidenceFixture make_synthetic_fixture();

} // namespace rocket3::visual_compare
