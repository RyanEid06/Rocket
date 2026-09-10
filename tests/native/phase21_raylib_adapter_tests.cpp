#include "rocket_raylib_adapter.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

static_assert(std::is_same_v<decltype(&rlv_window_framebuffer_width_f64),
                             double (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_window_framebuffer_height_f64),
                             double (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_render_texture_width_f64),
                             double (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_render_texture_height_f64),
                             double (*)(int64_t)>);

namespace {
int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase21 adapter failure: " << message << '\n';
}

bool near(double actual, double expected) {
  return std::abs(actual - expected) <= 0.0001;
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push title byte");
  }
  return id;
}
}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable deterministic backend");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");

  const int64_t title = textBuffer("WP21 virtual canvas");
  const int64_t window = rlv_window_open(800, 450, title);
  expect(window > 0, "open deterministic window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");

  expect(rlv_test_set_display_metrics(800, 600, 1600, 900, 0.0, 1.5,
                                      1, 0) == RLV_ERR_INVALID_ARGUMENT,
         "reject invalid framebuffer coordinate scale");
  expect(rlv_test_set_display_metrics(800, 600, 1600, 900, 2.0, 1.5,
                                      1, 0) == RLV_OK,
         "set non-uniform high-DPI display metrics");
  expect(near(rlv_window_framebuffer_width_f64(window), 1600.0) &&
             near(rlv_window_framebuffer_height_f64(window), 900.0),
         "expose framebuffer dimensions as Rocket Float-compatible values");

  const int64_t target = rlv_render_texture_load(window, 1920, 1080);
  expect(target > 0, "create logical-size render target");
  expect(near(rlv_render_texture_width_f64(target), 1920.0) &&
             near(rlv_render_texture_height_f64(target), 1080.0),
         "expose render-target dimensions as Rocket Float-compatible values");
  expect(rlv_render_texture_width_f64(target + 9999) == 0.0 &&
             rlv_render_texture_height_f64(target + 9999) == 0.0,
         "stale target dimensions are deterministic zero values");

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0, "begin framebuffer-coordinate frame");
  const int64_t drawsBefore = rlv_draw_count();
  expect(rlv_render_texture_draw_framebuffer(frame, target,
             0.0, 0.0, 1920.0, -1080.0,
             200.0, 150.0, 400.0, 300.0,
             0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_OK &&
             rlv_draw_count() == drawsBefore + 1,
         "draw framebuffer rectangle through production DPI conversion");
  const int64_t windowScissor =
      rlv_scissor_begin_framebuffer(frame, 200.0, 150.0, 400.0, 300.0);
  expect(windowScissor > 0 && rlv_test_scissor_x() == 100 &&
             rlv_test_scissor_y() == 100 &&
             rlv_test_scissor_width() == 200 &&
             rlv_test_scissor_height() == 200,
         "convert framebuffer scissor to non-uniform window coordinates");
  expect(rlv_scissor_end(windowScissor) == RLV_OK,
         "end framebuffer window scissor");
  const int64_t emptyWindowScissor =
      rlv_scissor_begin_framebuffer(frame, 1.0, 1.0, 0.0, 0.0);
  expect(emptyWindowScissor > 0 && rlv_test_scissor_width() == 0 &&
             rlv_test_scissor_height() == 0,
         "preserve zero-area framebuffer scissor after DPI conversion");
  expect(rlv_scissor_end(emptyWindowScissor) == RLV_OK,
         "end empty framebuffer window scissor");

  const int64_t targetScope = rlv_render_target_begin(frame, target);
  expect(targetScope > 0, "begin active render target");
  const int64_t targetScissor =
      rlv_scissor_begin_framebuffer(frame, 20.0, 30.0, 40.0, 50.0);
  expect(targetScissor > 0 && rlv_test_scissor_x() == 20 &&
             rlv_test_scissor_y() == 30 &&
             rlv_test_scissor_width() == 40 &&
             rlv_test_scissor_height() == 50,
         "preserve framebuffer coordinates inside active render target");
  expect(rlv_scissor_end(targetScissor) == RLV_OK,
         "end framebuffer target scissor");
  expect(rlv_render_target_end(targetScope) == RLV_OK,
         "end active render target");
  expect(rlv_end_drawing(frame) == RLV_OK,
         "end framebuffer-coordinate frame");

  expect(rlv_render_texture_unload(target) == RLV_OK,
         "unload logical render target");
  expect(rlv_render_texture_width_f64(target) == 0.0 &&
             rlv_render_texture_height_f64(target) == 0.0,
         "unloaded target dimensions cannot leak stale metadata");
  expect(rlv_window_close(window) == RLV_OK, "close deterministic window");
  return failures == 0 ? 0 : 1;
}
