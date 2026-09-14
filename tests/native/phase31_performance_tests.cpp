#include "evidence_schema.h"
#include "rocket_raylib_adapter.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;
using rocket3::visual_compare::PerformanceObservation;

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase31 performance failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push text byte");
  }
  return id;
}

struct Scene {
  std::filesystem::path root;
  std::vector<int64_t> buffers;
  int64_t window = 0;
  int64_t font = 0;
  int64_t target = 0;
  int64_t store = 0;
  int64_t textureReference = 0;
};

Scene prepareScene(const std::filesystem::path& artifactRoot) {
  Scene scene;
  scene.root = artifactRoot / "assets";
  std::error_code error;
  std::filesystem::remove_all(scene.root, error);
  std::filesystem::create_directories(scene.root, error);
  std::ofstream(scene.root / "card.png") << "deterministic texture fixture";

  scene.buffers = {
      textBuffer("WP31 steady card table"),
      textBuffer(scene.root.generic_string()),
      textBuffer("card"),
      textBuffer("card.png"),
      textBuffer("Rocket 3 performance")};
  scene.window = rlv_window_open(640, 360, scene.buffers[0]);
  scene.font = rlv_font_default(scene.window);
  scene.target = rlv_render_texture_load(scene.window, 320, 180);
  scene.store = rlv_asset_store_create_bounded(
      scene.window, 0, scene.buffers[1], 8);
  scene.textureReference = rlv_asset_texture_load(
      scene.store, scene.buffers[2], scene.buffers[3]);
  expect(scene.window > 0 && scene.font > 0 && scene.target > 0 &&
             scene.store > 0 && scene.textureReference > 0,
         "prepare final integrated scene resources");
  return scene;
}

bool runFrame(const Scene& scene) {
  const int64_t texture =
      rlv_asset_texture_borrow(rlv_asset_texture_lookup(
          scene.store, scene.buffers[2]));
  const int64_t frame = rlv_begin_drawing(scene.window);
  if (texture <= 0 || frame <= 0) return false;
  if (rlv_clear_background(frame, 18, 22, 31, 255) != RLV_OK) return false;
  const int64_t targetScope = rlv_render_target_begin(frame, scene.target);
  if (targetScope <= 0) return false;
  if (rlv_draw_rectangle(frame, 10, 10, 120, 70, 45, 92, 156, 255) !=
      RLV_OK) {
    return false;
  }
  if (rlv_render_target_end(targetScope) != RLV_OK) return false;
  if (rlv_render_texture_draw(frame, scene.target, 0.0, 0.0, 320.0,
                              180.0, 0.0, 0.0, 640.0, 360.0, 0.0, 0.0,
                              0.0, 255, 255, 255, 255) != RLV_OK) {
    return false;
  }
  const int64_t layout = rlv_font_measure(
      scene.font, scene.buffers[4], 18.0, 0.0, 1.2, 220.0, 60.0, 0,
      RLV_TEXT_OVERFLOW_CLIP);
  if (layout <= 0) return false;
  if (rlv_font_draw_layout(frame, scene.font, scene.buffers[4], 12.0, 12.0,
                           220.0, 60.0, 18.0, 0.0, 1.2,
                           RLV_TEXT_ALIGN_LEFT, RLV_TEXT_ALIGN_TOP, 0, 0,
                           RLV_TEXT_OVERFLOW_CLIP, 245, 247, 250, 255) !=
      RLV_OK) {
    return false;
  }
  return rlv_text_layout_destroy(layout) == RLV_OK &&
         rlv_end_drawing(frame) == RLV_OK;
}

void releaseScene(Scene& scene) {
  expect(rlv_asset_store_cleanup(scene.store) == RLV_OK,
         "clean asset store");
  expect(rlv_render_texture_unload(scene.target) == RLV_OK,
         "clean render target");
  expect(rlv_font_unload(scene.font) == RLV_OK, "clean default font");
  expect(rlv_window_close(scene.window) == RLV_OK, "close window");
  for (const int64_t buffer : scene.buffers) {
    expect(rlv_buffer_destroy(buffer) == RLV_OK, "clean scene buffer");
  }
  std::error_code error;
  std::filesystem::remove_all(scene.root, error);
}

PerformanceObservation observe(const Scene& scene) {
  const auto budget = rocket3::visual_compare::final_performance_budget();
  for (std::uint64_t index = 0; index < budget.warmup_frames; ++index) {
    expect(runFrame(scene), "complete warm-up frame");
  }
  expect(rlv_performance_reset() == RLV_OK,
         "reset counters after warm-up");

  std::vector<double> frameTimes;
  frameTimes.reserve(budget.measured_frames);
  for (std::uint64_t index = 0; index < budget.measured_frames; ++index) {
    const auto start = Clock::now();
    expect(runFrame(scene), "complete measured steady-state frame");
    const auto stop = Clock::now();
    frameTimes.push_back(
        std::chrono::duration<double, std::micro>(stop - start).count());
  }

  PerformanceObservation result;
  result.measured_frames = budget.measured_frames;
  result.native_allocations = rlv_performance_native_allocations();
  result.temporary_strings = rlv_performance_temporary_strings();
  result.layout_allocations = rlv_performance_layout_allocations();
  result.layout_recomputations = rlv_performance_layout_recomputations();
  result.text_measurements = rlv_performance_text_measurements();
  result.asset_lookups = rlv_performance_asset_lookups();
  result.ffi_calls = rlv_performance_ffi_calls();
  result.render_target_switches = rlv_render_target_switch_count();
  result.texture_uploads = rlv_performance_texture_uploads();
  result.peak_state_growth = rlv_performance_peak_state_growth();
  result.peak_cache_growth = rlv_performance_peak_cache_growth();
  result.mean_frame_time_us =
      std::accumulate(frameTimes.begin(), frameTimes.end(), 0.0) /
      static_cast<double>(frameTimes.size());
  result.maximum_frame_time_us =
      *std::max_element(frameTimes.begin(), frameTimes.end());
  return result;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) {
    std::cerr << "usage: rocket_phase31_performance_tests <artifact-root>\n";
    return 2;
  }
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable deterministic backend");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  Scene scene = prepareScene(argv[1]);
  const auto observation = observe(scene);
  const auto budget = rocket3::visual_compare::final_performance_budget();
  const auto violations =
      rocket3::visual_compare::performance_budget_violations(observation,
                                                              budget);
  for (const auto& violation : violations) {
    expect(false, violation);
  }
  expect(rlv_performance_state_entries() > 0,
         "report retained steady-state resources");
  expect(rlv_performance_cache_entries() == 3,
         "retain one warmed text measurement and two bounded asset entries");
  expect(rlv_text_layout_live_count() == 0,
         "release every per-frame layout handle");
  releaseScene(scene);
  expect(rlv_buffer_live_count() == 0 && rlv_texture_live_count() == 0 &&
             rlv_render_texture_live_count() == 0 &&
             rlv_font_live_count() == 0 &&
             rlv_asset_store_live_count() == 0,
         "leave no native resources live");
  if (failures != 0) {
    std::cerr << failures << " phase31 performance failure(s)\n";
    return 1;
  }
  std::cout << "WP31 performance budgets passed: "
            << observation.measured_frames << " steady-state frames, mean "
            << observation.mean_frame_time_us << " us, max "
            << observation.maximum_frame_time_us << " us; allocations "
            << observation.native_allocations << ", temporary strings "
            << observation.temporary_strings << ", layout allocations "
            << observation.layout_allocations << ", layout recomputations "
            << observation.layout_recomputations << ", text measurements "
            << observation.text_measurements << ", asset lookups "
            << observation.asset_lookups << ", FFI calls "
            << observation.ffi_calls << ", target switches "
            << observation.render_target_switches << ", texture uploads "
            << observation.texture_uploads << ", peak state growth "
            << observation.peak_state_growth << ", peak cache growth "
            << observation.peak_cache_growth << '\n';
  return 0;
}
