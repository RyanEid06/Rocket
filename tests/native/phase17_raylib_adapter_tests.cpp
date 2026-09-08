#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

#include <raylib.h>

static_assert(std::is_same_v<decltype(&rlv_shader_supported),
                             rocket_bool (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_load_memory),
                             int64_t (*)(int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_uniform),
                             int64_t (*)(int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_set_float),
                             int64_t (*)(int64_t, int64_t, double)>);
static_assert(std::is_same_v<decltype(&rlv_shader_set_int),
                             int64_t (*)(int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_set_vec2),
                             int64_t (*)(int64_t, int64_t, double, double)>);
static_assert(std::is_same_v<decltype(&rlv_shader_set_color),
                             int64_t (*)(int64_t, int64_t, int64_t, int64_t,
                                         int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_begin),
                             int64_t (*)(int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_shader_end),
                             int64_t (*)(int64_t)>);

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase17 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  }
  return id;
}

void deterministicShaderCycle() {
  const int64_t title = textBuffer("Shader Test");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open deterministic window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");
  expect(rlv_shader_supported(window) == 1, "shader capability is explicit");

  const int64_t emptyVertex = textBuffer("");
  const int64_t fragment = textBuffer("uniform float intensity; uniform int mode; uniform vec2 offset; uniform vec4 tint; uniform float singleton[1];");
  const int64_t emptyFragment = textBuffer("");
  expect(rlv_shader_load_memory(window, emptyVertex, emptyFragment) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject empty shader program");
  expect(rlv_test_set_shader_supported(0) == RLV_OK,
         "disable deterministic shader capability");
  expect(rlv_shader_load_memory(window, emptyVertex, fragment) ==
             RLV_ERR_UNAVAILABLE,
         "report unsupported shader backend");
  expect(rlv_test_set_shader_supported(1) == RLV_OK,
         "restore deterministic shader capability");
  const int64_t invalidSource = textBuffer("invalid_shader");
  expect(rlv_shader_load_memory(window, emptyVertex, invalidSource) ==
             RLV_ERR_INVALID_SHADER,
         "distinguish invalid shader source");
  expect(rlv_buffer_destroy(invalidSource) == RLV_OK,
         "destroy invalid shader source buffer");

  const int64_t shader = rlv_shader_load_memory(window, emptyVertex, fragment);
  expect(shader > 0, "load shader from checked source buffers");
  expect(rlv_shader_live_count() == 1, "track live shader");

  const int64_t missingPath = textBuffer("missing.frag");
  expect(rlv_shader_load_files(window, emptyVertex, missingPath) ==
             RLV_ERR_NOT_FOUND,
         "report missing shader file");
  expect(rlv_buffer_destroy(missingPath) == RLV_OK,
         "destroy missing path buffer");

  const int64_t intensityName = textBuffer("intensity");
  const int64_t modeName = textBuffer("mode");
  const int64_t offsetName = textBuffer("offset");
  const int64_t tintName = textBuffer("tint");
  const int64_t missingName = textBuffer("missing_uniform");
  const int64_t singletonName = textBuffer("singleton");
  const int64_t intensity =
      rlv_shader_uniform(shader, intensityName, RLV_SHADER_UNIFORM_FLOAT);
  const int64_t mode =
      rlv_shader_uniform(shader, modeName, RLV_SHADER_UNIFORM_INT);
  const int64_t offset =
      rlv_shader_uniform(shader, offsetName, RLV_SHADER_UNIFORM_VEC2);
  const int64_t tint =
      rlv_shader_uniform(shader, tintName, RLV_SHADER_UNIFORM_COLOR);
  expect(intensity > 0 && mode > 0 && offset > 0 && tint > 0,
         "create typed uniform tokens");
  expect(rlv_shader_uniform(shader, tintName, RLV_SHADER_UNIFORM_FLOAT) ==
             RLV_ERR_SHADER_TYPE,
         "reject a caller-mislabeled uniform type");
  expect(rlv_shader_uniform(shader, singletonName, RLV_SHADER_UNIFORM_FLOAT) ==
             RLV_ERR_SHADER_TYPE,
         "reject a deterministic one-element uniform array");
  expect(rlv_shader_uniform(shader, missingName, RLV_SHADER_UNIFORM_FLOAT) ==
             RLV_ERR_NOT_FOUND,
         "report missing uniform");
  expect(rlv_shader_uniform(shader, intensityName, 99) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject unreviewed uniform type");
  expect(rlv_buffer_destroy(intensityName) == RLV_OK &&
             rlv_buffer_destroy(modeName) == RLV_OK &&
             rlv_buffer_destroy(offsetName) == RLV_OK &&
             rlv_buffer_destroy(tintName) == RLV_OK &&
             rlv_buffer_destroy(singletonName) == RLV_OK &&
             rlv_buffer_destroy(missingName) == RLV_OK,
         "release uniform name buffers");

  expect(rlv_shader_set_float(shader, intensity, 0.75) == RLV_OK,
         "set finite Float uniform");
  expect(rlv_shader_set_int(shader, mode, 2) == RLV_OK,
         "set Int uniform");
  expect(rlv_shader_set_vec2(shader, offset, 4.0, -2.0) == RLV_OK,
         "set Vec2 uniform");
  expect(rlv_shader_set_color(shader, tint, 12, 34, 56, 200) == RLV_OK,
         "set Color uniform");
  expect(rlv_shader_set_int(shader, intensity, 1) == RLV_ERR_SHADER_TYPE,
         "reject mismatched uniform setter");
  expect(rlv_shader_set_float(shader, intensity,
             std::numeric_limits<double>::quiet_NaN()) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject non-finite uniform value");
  expect(rlv_shader_set_color(shader, tint, 256, 0, 0, 255) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject invalid Color uniform");

  const int64_t second =
      rlv_shader_load_memory(window, emptyVertex, fragment);
  expect(second > 0, "load second shader");
  expect(rlv_shader_set_float(second, intensity, 1.0) ==
             RLV_ERR_INVALID_ARGUMENT,
         "uniform token is bound to its shader");
  expect(rlv_buffer_destroy(emptyVertex) == RLV_OK &&
             rlv_buffer_destroy(emptyFragment) == RLV_OK &&
             rlv_buffer_destroy(fragment) == RLV_OK,
         "release shader source buffers");

  const int64_t target = rlv_render_texture_load(window, 320, 180);
  expect(target > 0, "create shader-pass render target");
  const int64_t frame = rlv_begin_drawing(window);
  const int64_t targetScope = rlv_render_target_begin(frame, target);
  const int64_t shaderScope = rlv_shader_begin(frame, shader);
  const int64_t nestedScope = rlv_shader_begin(frame, second);
  expect(targetScope > 0 && shaderScope > 0 && nestedScope > 0,
         "nest shader scopes inside a render target");
  expect(rlv_render_target_end(targetScope) == RLV_ERR_INVALID_STATE,
         "global scope order includes shaders");
  expect(rlv_shader_end(shaderScope) == RLV_ERR_INVALID_STATE,
         "reject out-of-order shader end");
  expect(rlv_shader_unload(second) == RLV_ERR_INVALID_STATE,
         "reject active shader unload");
  expect(rlv_shader_end(nestedScope) == RLV_OK,
         "end nested shader and restore parent");
  expect(rlv_shader_end(shaderScope) == RLV_OK, "end parent shader");
  expect(rlv_render_target_end(targetScope) == RLV_OK,
         "end render target after shader scopes");
  expect(rlv_end_drawing(frame) == RLV_OK, "end shader frame");

  const int64_t abortFrame = rlv_begin_drawing(window);
  const int64_t abortShader = rlv_shader_begin(abortFrame, shader);
  expect(abortShader > 0, "open shader scope for abort");
  expect(rlv_abort_drawing(abortFrame) == RLV_OK,
         "abort cleans shader scope");
  expect(rlv_shader_end(abortShader) == RLV_ERR_STALE_HANDLE,
         "aborted shader scope token is stale");
  expect(rlv_scope_depth() == 0, "shader abort leaves no scope");
  expect(rlv_shader_switch_count() == 7,
         "count begin/end/restore/abort shader transitions");

  expect(rlv_window_close(window) == RLV_ERR_RESOURCE_LIVE,
         "window owns live shaders");
  expect(rlv_shader_unload(shader) == RLV_OK, "unload first shader");
  expect(rlv_shader_set_float(shader, intensity, 1.0) ==
             RLV_ERR_STALE_HANDLE,
         "uniform becomes stale with shader");
  expect(rlv_shader_unload(shader) == RLV_ERR_STALE_HANDLE,
         "reject double shader unload");
  expect(rlv_shader_unload(second) == RLV_OK, "unload second shader");
  expect(rlv_shader_live_count() == 0 &&
             rlv_shader_uniform_live_count() == 0,
         "shader unload cleans uniform tokens");
  expect(rlv_render_texture_unload(target) == RLV_OK,
         "unload shader-pass render target");
  expect(rlv_window_close(window) == RLV_OK, "close deterministic window");
}

void nativeShaderCycle() {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  const int64_t title = textBuffer("WP17 native shader regression");
  const int64_t window = rlv_window_open(96, 64, title);
  expect(window > 0, "open hidden native shader window");
  expect(rlv_shader_supported(window) == 1,
         "native backend reports shader capability");

  const int64_t vertex = textBuffer("");
  const int64_t fragment = textBuffer(
      "#version 330\n"
      "in vec4 fragColor;\n"
      "out vec4 finalColor;\n"
      "uniform float intensity;\n"
      "uniform vec2 offset;\n"
      "uniform vec4 tint;\n"
      "uniform sampler2D unsupportedSampler;\n"
      "uniform float weights[1];\n"
      "void main(){\n"
      "  vec4 sampled = texture(unsupportedSampler, vec2(0.0))*0.000001;\n"
      "  float weighted = weights[0]*0.000001;\n"
      "  finalColor = vec4(tint.rgb*intensity, tint.a)*fragColor + vec4(offset,0.0,0.0) + sampled + vec4(weighted);\n"
      "}\n");
  const int64_t shader = rlv_shader_load_memory(window, vertex, fragment);
  expect(shader > 0, "compile real fragment shader");
  const int64_t solidFragment = textBuffer(
      "#version 330\n"
      "out vec4 finalColor;\n"
      "void main(){ finalColor = vec4(1.0, 0.0, 0.0, 1.0); }\n");
  const int64_t solidShader =
      rlv_shader_load_memory(window, vertex, solidFragment);
  expect(solidShader > 0, "compile nested solid-color shader");
  const int64_t invalidFragment = textBuffer(
      "#version 330\n"
      "in vec3 fragColor;\n"
      "out vec4 finalColor;\n"
      "void main(){ finalColor = vec4(fragColor, 1.0); }\n");
  expect(rlv_shader_load_memory(window, vertex, invalidFragment) ==
             RLV_ERR_INVALID_SHADER,
         "reject raylib default-shader fallback after compile failure");
  const char* validFilePath = "wp17-valid-fragment.fs";
  {
    std::ofstream validFile(validFilePath, std::ios::binary);
    validFile << "#version 330\n"
                 "in vec4 fragColor;\n"
                 "out vec4 finalColor;\n"
                 "void main(){ finalColor = fragColor; }\n";
  }
  const int64_t unreadableDirectory = textBuffer(".");
  const int64_t validFile = textBuffer(validFilePath);
  const int64_t fileShader =
      rlv_shader_load_files(window, vertex, validFile);
  expect(fileShader > 0, "load a valid fragment shader file");
  expect(rlv_shader_unload(fileShader) == RLV_OK,
         "unload a file-backed shader");
  expect(rlv_shader_load_files(window, unreadableDirectory, validFile) ==
             RLV_ERR_NOT_FOUND,
         "reject unreadable supplied stage instead of substituting default");
  expect(rlv_shader_load_files(window, validFile, unreadableDirectory) ==
             RLV_ERR_NOT_FOUND,
         "release first stage when the second stage is unreadable");
  expect(rlv_buffer_destroy(unreadableDirectory) == RLV_OK &&
             rlv_buffer_destroy(validFile) == RLV_OK,
         "destroy file-loading regression buffers");
  std::remove(validFilePath);
  const int64_t intensityName = textBuffer("intensity");
  const int64_t offsetName = textBuffer("offset");
  const int64_t tintName = textBuffer("tint");
  const int64_t samplerName = textBuffer("unsupportedSampler");
  const int64_t weightsName = textBuffer("weights");
  const int64_t intensity =
      rlv_shader_uniform(shader, intensityName, RLV_SHADER_UNIFORM_FLOAT);
  const int64_t offset =
      rlv_shader_uniform(shader, offsetName, RLV_SHADER_UNIFORM_VEC2);
  const int64_t tint =
      rlv_shader_uniform(shader, tintName, RLV_SHADER_UNIFORM_COLOR);
  expect(intensity > 0 && offset > 0 && tint > 0,
         "lookup real shader uniforms");
  expect(rlv_shader_uniform(shader, tintName, RLV_SHADER_UNIFORM_FLOAT) ==
             RLV_ERR_SHADER_TYPE,
         "reflect and reject a real GLSL type mismatch");
  expect(rlv_shader_uniform(shader, samplerName, RLV_SHADER_UNIFORM_INT) ==
             RLV_ERR_SHADER_TYPE,
         "reject an unreviewed sampler uniform");
  expect(rlv_shader_uniform(shader, weightsName, RLV_SHADER_UNIFORM_FLOAT) ==
             RLV_ERR_SHADER_TYPE,
         "reject an unreviewed uniform array");
  expect(rlv_shader_set_float(shader, intensity, 0.5) == RLV_OK,
         "set real float uniform");
  expect(rlv_shader_set_vec2(shader, offset, 0.0, 0.0) == RLV_OK,
         "set real vec2 uniform");
  expect(rlv_shader_set_color(shader, tint, 128, 200, 255, 255) == RLV_OK,
         "set real color uniform");

  const int64_t target = rlv_render_texture_load(window, 48, 32);
  const int64_t frame = rlv_begin_drawing(window);
  const int64_t targetScope = rlv_render_target_begin(frame, target);
  const int64_t shaderScope = rlv_shader_begin(frame, shader);
  expect(rlv_draw_rectangle(frame, 0, 0, 16, 32, 255, 255, 255, 255) == RLV_OK,
         "draw first parent-shader region");
  const int64_t solidScope = rlv_shader_begin(frame, solidShader);
  expect(rlv_draw_rectangle(frame, 16, 0, 16, 32, 255, 255, 255, 255) == RLV_OK,
         "draw nested-shader region");
  expect(rlv_shader_end(solidScope) == RLV_OK,
         "end nested shader and restore real parent");
  expect(rlv_draw_rectangle(frame, 32, 0, 16, 32, 255, 255, 255, 255) == RLV_OK,
         "draw restored parent-shader region");
  expect(target > 0 && frame > 0 && targetScope > 0 && shaderScope > 0 &&
             solidScope > 0,
         "begin real shader render-target pass");
  expect(rlv_shader_end(shaderScope) == RLV_OK, "end real shader scope");
  expect(rlv_render_target_end(targetScope) == RLV_OK,
         "end real target scope");
  expect(rlv_end_drawing(frame) == RLV_OK, "end real shader frame");
  const char* outputPath = "wp17-shader-output.png";
  const int64_t output = textBuffer(outputPath);
  expect(rlv_render_texture_save_png(window, target, output) == RLV_OK,
         "save real shader output");
  Image outputImage = LoadImage(outputPath);
  expect(IsImageValid(outputImage), "load saved shader output");
  if (IsImageValid(outputImage)) {
    const Color left = GetImageColor(outputImage, 8, 16);
    const Color middle = GetImageColor(outputImage, 24, 16);
    const Color right = GetImageColor(outputImage, 40, 16);
    expect(std::abs(static_cast<int>(left.r) - 64) <= 2 &&
               std::abs(static_cast<int>(left.g) - 100) <= 2 &&
               std::abs(static_cast<int>(left.b) - 128) <= 2,
           "apply typed uniforms to real shader output");
    expect(middle.r >= 253 && middle.g <= 2 && middle.b <= 2,
           "apply nested shader to real output");
    expect(std::abs(static_cast<int>(right.r) - static_cast<int>(left.r)) <= 1 &&
               std::abs(static_cast<int>(right.g) - static_cast<int>(left.g)) <= 1 &&
               std::abs(static_cast<int>(right.b) - static_cast<int>(left.b)) <= 1,
           "restore parent shader after nested scope");
    UnloadImage(outputImage);
  }
  expect(rlv_buffer_destroy(output) == RLV_OK,
         "destroy shader output path buffer");
  std::remove(outputPath);
  expect(rlv_render_texture_unload(target) == RLV_OK,
         "unload native target");
  expect(rlv_shader_unload(solidShader) == RLV_OK,
         "unload nested solid-color shader");
  expect(rlv_shader_unload(shader) == RLV_OK, "unload native shader");
  expect(rlv_window_close(window) == RLV_OK, "close hidden native window");
  for (int64_t bufferId : {title, vertex, fragment, solidFragment,
                           invalidFragment,
                           intensityName, offsetName, tintName, samplerName,
                           weightsName}) {
    expect(rlv_buffer_destroy(bufferId) == RLV_OK,
           "destroy native shader buffer");
  }
}

}  // namespace

int main(int argc, char**) {
  if (argc == 2) nativeShaderCycle();
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset test backend");
  deterministicShaderCycle();
  expect(rlv_buffer_live_count() == 0 && rlv_shader_live_count() == 0 &&
             rlv_shader_uniform_live_count() == 0 && rlv_scope_depth() == 0,
         "all shader resources are cleaned up");
  if (failures == 0) {
    std::cout << "phase17 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase17 adapter test failure(s)\n";
  return 1;
}
