#include "../../src/raylib/rocket_raylib_adapter.h"

#include <iostream>
#include <initializer_list>
#include <string_view>

namespace {
int failures = 0;

void expect(bool condition, std::string_view label) {
  if (!condition) {
    ++failures;
    std::cerr << "WP3 adapter failure: " << label << '\n';
  }
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) expect(rlv_buffer_push(id, byte) == RLV_OK, "buffer byte");
  return id;
}

}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK && rlv_test_reset() == RLV_OK,
         "start deterministic adapter");
  const int64_t title = textBuffer("WP3 native effects");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open window");
  expect(rlv_shader_supported(window), "query shader capability");
  expect(rlv_test_set_shader_supported(0) == RLV_OK &&
             !rlv_shader_supported(window), "detect unsupported shaders");
  const int64_t empty = textBuffer("");
  const int64_t invalid = textBuffer("invalid_shader");
  expect(rlv_shader_load_memory(window, empty, invalid) == RLV_ERR_UNAVAILABLE,
         "unsupported shader returns a controlled error");
  expect(rlv_test_set_shader_supported(1) == RLV_OK, "restore capability");
  expect(rlv_shader_load_memory(window, empty, invalid) == RLV_ERR_INVALID_SHADER,
         "reject invalid shader source");
  expect(rlv_shader_diagnostic_length() > 0 &&
             rlv_shader_diagnostic_byte(0) == 't' &&
             rlv_shader_diagnostic_byte(rlv_shader_diagnostic_length()) ==
                 RLV_ERR_INVALID_ARGUMENT,
         "retain bounded shader compiler diagnostic");
  const int64_t source = textBuffer(
      "uniform float intensity; uniform int mode; uniform vec2 offset; "
      "uniform vec4 tint;");
  const int64_t shader = rlv_shader_load_memory(window, empty, source);
  expect(shader > 0 && rlv_shader_diagnostic_length() == 0,
         "success clears previous diagnostic");
  const int64_t intensityName = textBuffer("intensity");
  const int64_t modeName = textBuffer("mode");
  const int64_t offsetName = textBuffer("offset");
  const int64_t tintName = textBuffer("tint");
  const int64_t intensity =
      rlv_shader_uniform(shader, intensityName, RLV_SHADER_UNIFORM_FLOAT);
  const int64_t mode = rlv_shader_uniform(shader, modeName, RLV_SHADER_UNIFORM_INT);
  const int64_t offset =
      rlv_shader_uniform(shader, offsetName, RLV_SHADER_UNIFORM_VEC2);
  const int64_t tint =
      rlv_shader_uniform(shader, tintName, RLV_SHADER_UNIFORM_COLOR);
  expect(intensity > 0 && mode > 0 && offset > 0 && tint > 0,
         "look up four typed uniforms");
  expect(rlv_shader_set_float(shader, intensity, 0.75) == RLV_OK &&
             rlv_shader_set_int(shader, mode, 2) == RLV_OK &&
             rlv_shader_set_vec2(shader, offset, 2.0, -1.0) == RLV_OK &&
             rlv_shader_set_color(shader, tint, 12, 34, 56, 200) == RLV_OK,
         "round-trip all supported uniform types");
  expect(rlv_shader_set_int(shader, intensity, 1) == RLV_ERR_SHADER_TYPE,
         "reject wrong uniform type");

  const int64_t target = rlv_render_texture_load(window, 320, 180);
  const int64_t frame = rlv_begin_drawing(window);
  for (int64_t blendMode : {RLV_BLEND_ALPHA, RLV_BLEND_ADDITIVE,
                            RLV_BLEND_MULTIPLIED, RLV_BLEND_ADD_COLORS,
                            RLV_BLEND_SUBTRACT_COLORS,
                            RLV_BLEND_ALPHA_PREMULTIPLIED}) {
    const int64_t scope = rlv_blend_begin(frame, blendMode);
    expect(scope > 0 && rlv_blend_end(scope) == RLV_OK,
           "accept every documented blend mode");
  }
  const int64_t targetScope = rlv_render_target_begin(frame, target);
  const int64_t blend = rlv_blend_begin(frame, RLV_BLEND_ADDITIVE);
  const int64_t shaderScope = rlv_shader_begin(frame, shader);
  expect(targetScope > 0 && blend > 0 && shaderScope > 0,
         "compose target, blend and shader scopes");
  expect(rlv_blend_end(blend) == RLV_ERR_INVALID_STATE &&
             rlv_render_target_end(targetScope) == RLV_ERR_INVALID_STATE &&
             rlv_end_drawing(frame) == RLV_ERR_INVALID_STATE,
         "reject illegal nesting and unfinished frame");
  expect(rlv_shader_end(shaderScope) == RLV_OK &&
             rlv_shader_end(shaderScope) == RLV_ERR_STALE_HANDLE &&
             rlv_blend_end(blend) == RLV_OK &&
             rlv_blend_end(blend) == RLV_ERR_STALE_HANDLE &&
             rlv_render_target_end(targetScope) == RLV_OK,
         "end scopes exactly once in reverse order");
  const int64_t outputShader = rlv_shader_begin(frame, shader);
  expect(outputShader > 0 &&
             rlv_render_texture_draw_framebuffer(frame, target, 0, 0, 320,
                 -180, 0, 0, 640, 360, 0, 0, 0, 255, 255, 255, 255) == RLV_OK &&
             rlv_shader_end(outputShader) == RLV_OK &&
             rlv_end_drawing(frame) == RLV_OK,
         "post-process a render target to the output");
  expect(rlv_blend_begin(frame, RLV_BLEND_ALPHA) == RLV_ERR_STALE_HANDLE &&
             rlv_shader_begin(frame, shader) == RLV_ERR_STALE_HANDLE,
         "reject stale frames");
  const int64_t abortFrame = rlv_begin_drawing(window);
  const int64_t abortBlend = rlv_blend_begin(abortFrame, RLV_BLEND_MULTIPLIED);
  expect(rlv_abort_drawing(abortFrame) == RLV_OK &&
             rlv_blend_end(abortBlend) == RLV_ERR_STALE_HANDLE,
         "abort invalidates blend scope");
  expect(rlv_render_texture_unload(target) == RLV_OK &&
             rlv_shader_unload(shader) == RLV_OK &&
             rlv_shader_unload(shader) == RLV_ERR_STALE_HANDLE &&
             rlv_window_close(window) == RLV_OK,
         "clean up resources and reject double unload");
  for (int64_t id : {title, empty, invalid, source, intensityName, modeName,
                     offsetName, tintName}) {
    expect(rlv_buffer_destroy(id) == RLV_OK, "release buffer");
  }
  expect(rlv_buffer_live_count() == 0 && rlv_shader_live_count() == 0 &&
             rlv_shader_uniform_live_count() == 0 && rlv_scope_depth() == 0,
         "leave no live adapter resources");
  return failures == 0 ? 0 : 1;
}
