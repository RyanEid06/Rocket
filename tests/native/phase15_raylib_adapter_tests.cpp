#include "rocket_raylib_adapter.h"

#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string_view>
#include <type_traits>

static_assert(std::is_same_v<decltype(&rlv_texture_set_filter),
                             int64_t (*)(int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_texture_get_filter),
                             int64_t (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_texture_filter_supported),
                             rocket_bool (*)(int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_texture_max_anisotropy),
                             double (*)(void)>);
static_assert(std::is_same_v<decltype(&rlv_texture_draw_pro),
                             int64_t (*)(int64_t, int64_t,
                                         double, double, double, double,
                                         double, double, double, double,
                                         double, double, double,
                                         int64_t, int64_t, int64_t, int64_t)>);
static_assert(std::is_same_v<decltype(&rlv_test_set_anisotropy),
                             int64_t (*)(int64_t)>);

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase15 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  }
  return id;
}

void textureFilterCycle() {
  const int64_t title = textBuffer("Texture Filter Test");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open window for filter test");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title");

  const int64_t path = textBuffer("assets/sprite.png");
  const int64_t texture = rlv_texture_load(window, path);
  expect(texture > 0, "load texture");
  expect(rlv_buffer_destroy(path) == RLV_OK, "destroy path buffer");

  // Initial filter is point
  expect(rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_POINT,
         "default filter is point");

  // Basic filters are always supported
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_POINT) == 1,
         "point filter supported");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_BILINEAR) == 1,
         "bilinear filter supported");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_TRILINEAR) == 1,
         "trilinear filter supported");

  expect(rlv_texture_set_filter(window, texture, RLV_TEXTURE_FILTER_BILINEAR) == RLV_OK,
         "set bilinear filter");
  expect(rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_BILINEAR,
         "verify bilinear filter");

  expect(rlv_texture_set_filter(window, texture, RLV_TEXTURE_FILTER_TRILINEAR) == RLV_OK,
         "set trilinear filter");
  expect(rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_TRILINEAR,
         "verify trilinear filter");

  // Test anisotropic support with simulated levels
  expect(rlv_test_set_anisotropy(16) == RLV_OK, "set anisotropy level 16");
  expect(rlv_texture_max_anisotropy() >= 16.0, "max anisotropy query reflects level");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_4X) == 1,
         "4x aniso supported at 16");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_8X) == 1,
         "8x aniso supported at 16");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_16X) == 1,
         "16x aniso supported at 16");
  expect(rlv_texture_set_filter(window, texture, RLV_TEXTURE_FILTER_ANISOTROPIC_16X) == RLV_OK,
         "set 16x aniso filter");
  expect(rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_ANISOTROPIC_16X,
         "verify 16x aniso filter");

  // Simulate hardware without anisotropic filtering
  expect(rlv_test_set_anisotropy(0) == RLV_OK, "set anisotropy level 0");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_4X) == 0,
         "4x aniso unsupported at 0");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_8X) == 0,
         "8x aniso unsupported at 0");
  expect(rlv_texture_filter_supported(RLV_TEXTURE_FILTER_ANISOTROPIC_16X) == 0,
         "16x aniso unsupported at 0");
  expect(rlv_texture_set_filter(window, texture, RLV_TEXTURE_FILTER_ANISOTROPIC_4X) == RLV_ERR_UNAVAILABLE,
         "reject unsupported aniso filter");

  // Restore anisotropy capability
  expect(rlv_test_set_anisotropy(16) == RLV_OK, "restore anisotropy level 16");

  // Invalid arguments and error handling
  expect(rlv_texture_set_filter(window, texture, -1) == RLV_ERR_INVALID_ARGUMENT,
         "reject negative filter mode");
  expect(rlv_texture_set_filter(window, texture, 99) == RLV_ERR_INVALID_ARGUMENT,
         "reject out-of-range filter mode");
  expect(rlv_texture_set_filter(window, 99999, RLV_TEXTURE_FILTER_POINT) == RLV_ERR_STALE_HANDLE,
         "reject stale texture handle in set_filter");
  expect(rlv_texture_get_filter(99999) == RLV_ERR_STALE_HANDLE,
         "reject stale texture handle in get_filter");
  expect(rlv_texture_set_filter(99999, texture, RLV_TEXTURE_FILTER_POINT) == RLV_ERR_STALE_HANDLE,
         "reject stale window handle in set_filter");

  expect(rlv_texture_unload(texture) == RLV_OK, "unload texture");
  expect(rlv_window_close(window) == RLV_OK, "close window");
}

void textureDrawProCycle() {
  const int64_t title = textBuffer("Texture Draw Pro Test");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open window for draw_pro test");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title");

  const int64_t path = textBuffer("assets/player.png");
  const int64_t texture = rlv_texture_load(window, path);
  expect(texture > 0, "load texture for draw_pro");
  expect(rlv_buffer_destroy(path) == RLV_OK, "destroy path");

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0, "begin frame");

  const int64_t beforeDraws = rlv_draw_count();

  // Full source draw with rotation and pivot
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 64.0, 64.0,
                              100.0, 100.0, 128.0, 128.0,
                              64.0, 64.0, 45.0,
                              255, 255, 255, 255) == RLV_OK,
         "draw_pro full quad with rotation and pivot");

  // Partial sub-rectangle draw
  expect(rlv_texture_draw_pro(frame, texture,
                              16.0, 16.0, 32.0, 32.0,
                              200.0, 150.0, 64.0, 64.0,
                              0.0, 0.0, 0.0,
                              200, 220, 240, 255) == RLV_OK,
         "draw_pro sprite sub-region");

  // Negative dimensions (texture flipping)
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, -64.0, 64.0,
                              300.0, 100.0, 64.0, 64.0,
                              0.0, 0.0, 0.0,
                              255, 255, 255, 255) == RLV_OK,
         "draw_pro horizontally flipped");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 64.0, -64.0,
                              400.0, 100.0, 64.0, 64.0,
                              0.0, 0.0, 0.0,
                              255, 255, 255, 255) == RLV_OK,
         "draw_pro vertically flipped");

  expect(rlv_draw_count() == beforeDraws + 4, "draw count incremented by 4");

  // Rejections for invalid geometry / out-of-bounds
  const double nan = std::numeric_limits<double>::quiet_NaN();
  const double inf = std::numeric_limits<double>::infinity();

  expect(rlv_texture_draw_pro(frame, texture,
                              nan, 0.0, 64.0, 64.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject NaN sourceX");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, inf, 64.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject Inf sourceWidth");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, nan, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject NaN destWidth");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 64.0, 64.0,
                              nan, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject NaN originX");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, inf, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject Inf rotation");

  // Zero-sized source
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 0.0, 64.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject zero sourceWidth");

  // Negative source coordinate
  expect(rlv_texture_draw_pro(frame, texture,
                              -5.0, 0.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject negative sourceX");

  // Out-of-bounds source rectangle (texture size is 64x64)
  expect(rlv_texture_draw_pro(frame, texture,
                              40.0, 0.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject source rect exceeding width");
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 50.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject source rect exceeding height");

  // Invalid color
  expect(rlv_texture_draw_pro(frame, texture,
                              0.0, 0.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 300, 255, 255, 255) == RLV_ERR_INVALID_ARGUMENT,
         "reject out-of-range red");

  // Rejection on stale handles
  expect(rlv_texture_draw_pro(99999, texture,
                              0.0, 0.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_STALE_HANDLE,
         "reject invalid frame handle");
  expect(rlv_texture_draw_pro(frame, 99999,
                              0.0, 0.0, 32.0, 32.0,
                              0.0, 0.0, 64.0, 64.0,
                              0.0, 0.0, 0.0, 255, 255, 255, 255) == RLV_ERR_STALE_HANDLE,
         "reject invalid texture handle");

  expect(rlv_end_drawing(frame) == RLV_OK, "end frame");
  expect(rlv_texture_unload(texture) == RLV_OK, "unload texture");
  expect(rlv_window_close(window) == RLV_OK, "close window");
}

}  // namespace

int main() {
  expect(rlv_version_major() == 6 && rlv_version_minor() == 0,
         "adapter is compiled against raylib 6.0");
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset test backend");

  textureFilterCycle();
  textureDrawProCycle();

  expect(rlv_buffer_live_count() == 0 && rlv_texture_live_count() == 0 &&
             rlv_font_live_count() == 0 && rlv_sound_live_count() == 0,
         "all native registries are empty");

  if (failures == 0) {
    std::cout << "phase15 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase15 adapter test failure(s)\n";
  return 1;
}
