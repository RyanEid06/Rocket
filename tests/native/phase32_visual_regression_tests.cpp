#include "canonical_scenes.h"
#include "comparator.h"
#include "image_io.h"
#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <filesystem>
#include <iostream>
#include <set>
#include <string>
#include <string_view>

#include <raylib.h>

namespace {

using namespace rocket3::visual_compare;

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase32 visual failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t buffer = rlv_buffer_create();
  for (const unsigned char byte : value) {
    expect(rlv_buffer_push(buffer, byte) == RLV_OK,
           "append byte to native capture buffer");
  }
  return buffer;
}

void verifyNativeCapture(const std::filesystem::path& artifactRoot) {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  const int64_t title = textBuffer("Rocket WP32 native capture");
  const int64_t window = rlv_window_open(32, 24, title);
  expect(window > 0, "open hidden window for native screenshot capture");
  if (window <= 0) {
    rlv_buffer_destroy(title);
    return;
  }
  const int64_t target = rlv_render_texture_load(window, 16, 12);
  const int64_t frame = rlv_begin_drawing(window);
  const int64_t scope = rlv_render_target_begin(frame, target);
  expect(target > 0 && frame > 0 && scope > 0,
         "open native render-target capture scope");
  expect(rlv_clear_background(frame, 12, 34, 56, 255) == RLV_OK,
         "clear native capture target");
  expect(rlv_draw_rectangle(frame, 2, 2, 4, 3, 230, 80, 96, 255) == RLV_OK,
         "draw native capture marker");
  expect(rlv_render_target_end(scope) == RLV_OK,
         "close native render-target capture scope");
  expect(rlv_end_drawing(frame) == RLV_OK, "finish native capture frame");

  const auto capturePath = artifactRoot / "native-capture.png";
  const int64_t path = textBuffer(capturePath.string());
  expect(rlv_render_texture_save_png(window, target, path) == RLV_OK,
         "save native render target as PNG");
  const ImageResult capture = read_png(capturePath);
  expect(capture.ok && capture.image.width == 16 && capture.image.height == 12,
         "load the native screenshot at its exact dimensions");
  if (capture.ok) {
    bool foundBackground = false;
    bool foundMarker = false;
    for (std::size_t offset = 0; offset < capture.image.pixels.size();
         offset += 4) {
      const auto* pixel = capture.image.pixels.data() + offset;
      foundBackground = foundBackground ||
                        (pixel[0] == 12 && pixel[1] == 34 && pixel[2] == 56);
      foundMarker = foundMarker ||
                    (pixel[0] == 230 && pixel[1] == 80 && pixel[2] == 96);
    }
    expect(foundBackground && foundMarker,
           "preserve known native colors in the captured PNG");
  }

  expect(rlv_render_texture_unload(target) == RLV_OK,
         "unload native capture target");
  expect(rlv_window_close(window) == RLV_OK, "close native capture window");
  expect(rlv_buffer_destroy(path) == RLV_OK,
         "destroy native capture path buffer");
  expect(rlv_buffer_destroy(title) == RLV_OK,
         "destroy native capture title buffer");
}

void verifyGoldenUpdatePolicy(const std::filesystem::path& artifactRoot,
                              const ImageRgba& image) {
  GoldenApproval proposed;
  proposed.status = GoldenApproval::Status::Proposed;
  proposed.approval_id = "wp32-proposal";
  proposed.scene_key = "scene:policy";
  proposed.baseline_hash = "sha256:proposal";
  std::string error;
  const auto candidate = artifactRoot / "review-only-golden.ppm";
  expect(!update_approved_golden(candidate, image, proposed, error),
         "reject an unapproved golden update");
  expect(!std::filesystem::exists(candidate),
         "never write an unapproved golden reference");

  proposed.status = GoldenApproval::Status::Approved;
  proposed.approved_by = "WP32 fixture review";
  proposed.approved_at = "2026-09-14";
  proposed.update_allowed = true;
  error.clear();
  expect(update_approved_golden(candidate, image, proposed, error),
         "allow an explicit reviewed golden update");
  expect(std::filesystem::exists(candidate),
         "write an explicitly approved golden candidate");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3) {
    std::cerr << "usage: rocket_phase32_visual_regression_tests "
                 "<golden-root> <artifact-root>\n";
    return 2;
  }
  const std::filesystem::path goldenRoot = argv[1];
  const std::filesystem::path artifactRoot = argv[2];
  std::error_code filesystemError;
  std::filesystem::remove_all(artifactRoot, filesystemError);
  std::filesystem::create_directories(artifactRoot, filesystemError);

  const std::set<std::string> requiredCoverage = {
      "geometry", "rounded-geometry", "outlines", "rings-sectors",
      "gradients", "typography-alignments", "transformed-textures",
      "filtering", "clipping", "blending", "shaders",
      "virtual-canvas", "layouts", "anchors", "control-states",
      "premium-showcase"};
  std::set<std::string> observedCoverage;
  const auto scenes = render_canonical_scenes();
  expect(scenes.size() == 4, "render the complete four-scene canonical suite");
  const bool strictWindows = std::string_view(ROCKET_NATIVE_TARGET) ==
                             "windows-x64";
  for (const auto& scene : scenes) {
    expect(scene.image.valid(), "produce a structurally valid RGBA scene");
    observedCoverage.insert(scene.coverage.begin(), scene.coverage.end());
    if (!strictWindows && !scene.portability_subset) continue;

    const ImageResult golden =
        read_ppm_reference(goldenRoot / (scene.name + ".ppm"));
    expect(golden.ok, "load approved canonical golden reference");
    if (!golden.ok) continue;
    const Comparison comparison = compare(
        golden.image.pixels, scene.image.pixels, scene.image.width,
        scene.image.height, 0);
    expect(comparison.ok, "compare equal-size canonical buffers");
    const Tolerance exact{0, 0.0, 0.0};
    expect(within_tolerance(comparison, exact),
           "match the approved deterministic golden exactly");

    std::string error;
    const auto sceneArtifacts = artifactRoot / scene.name;
    expect(write_comparison_artifacts(sceneArtifacts, scene.image,
                                      comparison, error),
           "write generated, difference, heatmap, and metrics artifacts");
    const ImageResult roundTrip = read_png(sceneArtifacts / "generated.png");
    expect(roundTrip.ok && roundTrip.image.width == scene.image.width &&
               roundTrip.image.height == scene.image.height &&
               roundTrip.image.pixels == scene.image.pixels,
           "round-trip generated RGBA through PNG without loss");
  }
  expect(observedCoverage == requiredCoverage,
         "cover every required canonical visual feature group");
  if (!scenes.empty()) {
    verifyGoldenUpdatePolicy(artifactRoot, scenes.front().image);
  }
  if (strictWindows) {
    verifyNativeCapture(artifactRoot);
  }
  if (failures != 0) {
    std::cerr << failures << " phase32 visual failure(s)\n";
    return 1;
  }
  std::cout << "WP32 visual regression passed " << scenes.size()
            << " canonical scenes on " << ROCKET_NATIVE_TARGET
            << (strictWindows ? " (strict suite)" : " (portability subset)")
            << '\n';
  return 0;
}
