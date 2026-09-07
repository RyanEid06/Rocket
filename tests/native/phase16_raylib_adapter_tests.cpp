#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

#include <raylib.h>

static_assert(std::is_same_v<decltype(&rlv_render_texture_load),
                             int64_t (*)(int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_render_target_begin),
                             int64_t (*)(int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_render_target_end),
                             int64_t (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_abort_drawing),
                             int64_t (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_scissor_begin),
                             int64_t (*)(int64_t, double, double, double,
                                         double)>);
static_assert(std::is_same_v<decltype(&rlv_scissor_end),
                             int64_t (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_blend_begin),
                             int64_t (*)(int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_blend_end),
                             int64_t (*)(int64_t)>);

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase16 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  }
  return id;
}

void renderScopeCycle() {
  const int64_t title = textBuffer("Render Scope Test");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");

  const int64_t first = rlv_render_texture_load(window, 320, 180);
  const int64_t second = rlv_render_texture_load(window, 160, 90);
  expect(first > 0 && second > 0, "create virtual render targets");
  expect(rlv_render_texture_width(first) == 320 &&
             rlv_render_texture_height(first) == 180,
         "preserve virtual resolution");
  expect(rlv_render_texture_live_count() == 2, "track live targets");
  expect(rlv_render_texture_load(window, 0, 90) == RLV_ERR_INVALID_ARGUMENT,
         "reject zero target width");
  expect(rlv_render_texture_load(99999, 10, 10) == RLV_ERR_STALE_HANDLE,
         "reject stale target window");

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0, "begin frame");
  const int64_t outer = rlv_render_target_begin(frame, first);
  const int64_t inner = rlv_render_target_begin(frame, second);
  expect(outer > 0 && inner > 0, "nest render targets");
  expect(rlv_render_target_end(outer) == RLV_ERR_INVALID_STATE,
         "reject out-of-order target end");
  expect(rlv_render_texture_unload(first) == RLV_ERR_INVALID_STATE,
         "reject unload while target scope is active");
  expect(rlv_render_target_end(inner) == RLV_OK, "end nested target");
  expect(rlv_render_target_end(outer) == RLV_OK, "end outer target");
  expect(rlv_render_target_end(outer) == RLV_ERR_STALE_HANDLE,
         "reject stale target scope");

  const int64_t scissor = rlv_scissor_begin(frame, 10, 20, 100, 80);
  const int64_t nestedScissor = rlv_scissor_begin(frame, 50, 0, 100, 50);
  expect(scissor > 0 && nestedScissor > 0, "nest scissors");
  expect(rlv_test_scissor_x() == 50 && rlv_test_scissor_y() == 20 &&
             rlv_test_scissor_width() == 60 &&
             rlv_test_scissor_height() == 30,
         "nested scissors use deterministic intersection");
  expect(rlv_scissor_end(scissor) == RLV_ERR_INVALID_STATE,
         "reject out-of-order scissor end");
  expect(rlv_scissor_end(nestedScissor) == RLV_OK, "end nested scissor");
  expect(rlv_test_scissor_x() == 10 && rlv_test_scissor_y() == 20,
         "restore parent scissor");
  expect(rlv_scissor_end(scissor) == RLV_OK, "end outer scissor");
  expect(rlv_scissor_begin(frame, 0, 0, -1, 10) == RLV_ERR_INVALID_ARGUMENT,
         "reject negative scissor size");

  const int64_t blend = rlv_blend_begin(frame, RLV_BLEND_ADDITIVE);
  const int64_t nestedBlend = rlv_blend_begin(frame, RLV_BLEND_MULTIPLIED);
  expect(blend > 0 && nestedBlend > 0, "nest blend modes");
  expect(rlv_test_blend_mode() == RLV_BLEND_MULTIPLIED,
         "report active nested blend");
  expect(rlv_blend_end(blend) == RLV_ERR_INVALID_STATE,
         "reject out-of-order blend end");
  expect(rlv_blend_end(nestedBlend) == RLV_OK, "end nested blend");
  expect(rlv_test_blend_mode() == RLV_BLEND_ADDITIVE,
         "restore parent blend");
  expect(rlv_blend_end(blend) == RLV_OK, "end outer blend");
  expect(rlv_blend_begin(frame, 99) == RLV_ERR_INVALID_ARGUMENT,
         "reject unreviewed blend mode");

  expect(rlv_render_texture_draw(frame, first,
             0.0, 0.0, 320.0, -180.0,
             0.0, 0.0, 640.0, 360.0,
             0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_OK,
         "composite a virtual target with vertical correction");
  expect(rlv_render_texture_draw(frame, first,
             0.0, 0.0, 400.0, 180.0,
             0.0, 0.0, 640.0, 360.0,
             0.0, 0.0, 0.0, 255, 255, 255, 255) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject target source outside bounds");

  const int64_t mixed = rlv_render_target_begin(frame, first);
  const int64_t mixedScissor = rlv_scissor_begin(frame, 0, 0, 20, 20);
  expect(rlv_render_target_end(mixed) == RLV_ERR_INVALID_STATE,
         "enforce global LIFO order across scope kinds");
  expect(rlv_end_drawing(frame) == RLV_ERR_INVALID_STATE,
         "reject ending frame with live scopes");
  expect(rlv_scissor_end(mixedScissor) == RLV_OK, "close mixed scissor");
  expect(rlv_render_target_end(mixed) == RLV_OK, "close mixed target");
  expect(rlv_scope_depth() == 0, "all scopes closed");
  expect(rlv_end_drawing(frame) == RLV_OK, "end frame");

  const int64_t abortFrame = rlv_begin_drawing(window);
  const int64_t abortTarget = rlv_render_target_begin(abortFrame, first);
  const int64_t abortScissor = rlv_scissor_begin(abortFrame, 0, 0, 20, 20);
  const int64_t abortBlend = rlv_blend_begin(abortFrame, RLV_BLEND_ALPHA);
  expect(abortTarget > 0 && abortScissor > 0 && abortBlend > 0,
         "open scopes for abort cleanup");
  expect(rlv_abort_drawing(abortFrame) == RLV_OK,
         "abort frame deterministically cleans every scope");
  expect(rlv_scope_depth() == 0, "abort leaves no live scope");
  expect(rlv_blend_end(abortBlend) == RLV_ERR_STALE_HANDLE,
         "abort invalidates old scope tokens");

  const int64_t path = textBuffer("out/render-scope.png");
  expect(rlv_render_texture_save_png(window, first, path) == RLV_OK,
         "save render target screenshot");
  expect(rlv_buffer_destroy(path) == RLV_OK, "destroy screenshot path");
  expect(rlv_render_texture_save_png(window, first, 99999) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject stale screenshot path buffer");

  expect(rlv_render_target_switch_count() == 9,
         "count target transitions including parent restoration");
  expect(rlv_scissor_switch_count() == 9,
         "count nested scissor transitions and restoration");
  expect(rlv_blend_switch_count() == 7,
         "count nested blend transitions and restoration");
  expect(rlv_screenshot_count() == 1, "count screenshots");

  expect(rlv_render_texture_unload(first) == RLV_OK, "unload first target");
  expect(rlv_render_texture_unload(first) == RLV_ERR_STALE_HANDLE,
         "reject stale target unload");
  expect(rlv_render_texture_unload(second) == RLV_OK, "unload second target");
  expect(rlv_window_close(window) == RLV_OK, "close window");
}

void nativeRenderCycle(std::string_view outputPath) {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  const int64_t title = textBuffer("WP16 native render-scope regression");
  const int64_t window = rlv_window_open(96, 64, title);
  expect(window > 0, "open hidden native window");
  const int64_t target = rlv_render_texture_load(window, 48, 32);
  expect(target > 0, "create native render texture");
  const int64_t frame = rlv_begin_drawing(window);
  const int64_t targetScope = rlv_render_target_begin(frame, target);
  const int64_t scissorScope = rlv_scissor_begin(frame, 4.0, 4.0, 40.0, 24.0);
  const int64_t blendScope = rlv_blend_begin(frame, RLV_BLEND_ADDITIVE);
  expect(targetScope > 0 && scissorScope > 0 && blendScope > 0,
         "open native render scopes");
  expect(rlv_clear_background(frame, 16, 24, 40, 255) == RLV_OK,
         "clear native render target");
  expect(rlv_blend_end(blendScope) == RLV_OK, "end native blend");
  expect(rlv_scissor_end(scissorScope) == RLV_OK, "end native scissor");
  expect(rlv_render_target_end(targetScope) == RLV_OK, "end native target");
  expect(rlv_render_texture_draw(frame, target,
             0.0, 0.0, 48.0, -32.0,
             0.0, 0.0, 96.0, 64.0,
             0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_OK,
         "composite native render target");
  expect(rlv_end_drawing(frame) == RLV_OK, "end native frame");
  const int64_t path = textBuffer(outputPath);
  expect(rlv_render_texture_save_png(window, target, path) == RLV_OK,
         "save native render texture PNG");
  expect(FileExists(outputPath.data()) && GetFileLength(outputPath.data()) > 0,
         "native screenshot exists and is nonempty");
  expect(rlv_render_texture_unload(target) == RLV_OK,
         "unload native render texture");
  expect(rlv_window_close(window) == RLV_OK, "close hidden native window");
  expect(rlv_buffer_destroy(path) == RLV_OK, "destroy native output path");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy native title");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2) nativeRenderCycle(argv[1]);
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset test backend");
  renderScopeCycle();
  expect(rlv_render_texture_live_count() == 0 && rlv_scope_depth() == 0,
         "all render resources and scopes are cleaned up");
  if (failures == 0) {
    std::cout << "phase16 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase16 adapter test failure(s)\n";
  return 1;
}
