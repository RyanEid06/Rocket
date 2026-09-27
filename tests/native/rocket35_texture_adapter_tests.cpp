#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "WP2 adapter failure: " << message << '\n';
}

int64_t buffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "encode asset path");
  }
  return id;
}
}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable deterministic backend");
  expect(rlv_test_reset() == RLV_OK, "reset backend");
  const int64_t title = buffer("WP2 texture pipeline");
  const int64_t window = rlv_window_open_quality(800, 600, title, 1, 1, 1);
  expect(window > 0, "open high-DPI window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "release title buffer");
  expect(rlv_test_set_display_metrics(800, 600, 1600, 1200, 2.0, 2.0,
                                      1, 0) == RLV_OK, "set high-DPI metrics");
  expect(rlv_window_framebuffer_width(window) == 1600 &&
             rlv_window_framebuffer_height(window) == 1200,
         "query physical framebuffer size");
  expect(rlv_window_logical_width(window) == 800 &&
             rlv_window_logical_height(window) == 600 &&
             rlv_window_dpi_scale_x(window) == 2.0 &&
             rlv_window_dpi_scale_y(window) == 2.0,
         "keep logical size distinct from high-DPI framebuffer");
  const int64_t firstRevision = rlv_window_display_revision(window);
  expect(rlv_window_set_size(window, 1000, 700) == RLV_OK &&
             rlv_window_logical_width(window) == 1000 &&
             rlv_window_framebuffer_width(window) == 2000 &&
             rlv_window_framebuffer_height(window) == 1400 &&
             rlv_window_display_revision(window) > firstRevision,
         "normal resize retains high-DPI scale");
  // A compositor may report the old framebuffer for several event pumps or
  // clamp a requested size. The test seam models each observed transition.
  expect(rlv_test_set_display_metrics(1000, 700, 1600, 1200, 2.0, 2.0,
                                      1, 0) == RLV_OK &&
             rlv_window_framebuffer_width(window) == 1600,
         "delayed framebuffer resize remains observable");
  expect(rlv_test_set_display_metrics(920, 660, 1840, 1320, 2.0, 2.0,
                                      1, 0) == RLV_OK &&
             rlv_window_logical_width(window) == 920 &&
             rlv_window_framebuffer_width(window) == 1840,
         "compositor-clamped dimensions remain authoritative");
  for (int size = 0; size < 8; ++size) {
    const int64_t logical = 800 + size * 20;
    expect(rlv_window_set_size(window, logical, 600) == RLV_OK &&
               rlv_window_logical_width(window) == logical &&
               rlv_window_framebuffer_width(window) == logical * 2,
           "repeated resize keeps logical and physical metrics in sync");
  }
  expect(rlv_window_set_size(window, 800, 600) == RLV_OK,
         "restore reference framebuffer before target cycles");

  const int64_t path = buffer("assets/card-atlas.png");
  for (int cycle = 0; cycle < 64; ++cycle) {
    const int64_t texture = rlv_texture_load(window, path);
    expect(texture > 0, "load texture");
    expect(rlv_texture_width(texture) == 64 &&
               rlv_texture_height(texture) == 64, "query texture dimensions");
    expect(rlv_texture_set_filter(window, texture,
                RLV_TEXTURE_FILTER_BILINEAR) == RLV_OK &&
               rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_BILINEAR,
           "switch and query bilinear filtering");
    expect(rlv_test_set_anisotropy(0) == RLV_OK &&
               !rlv_texture_filter_supported(
                   RLV_TEXTURE_FILTER_ANISOTROPIC_16X),
           "report unavailable anisotropy");
    expect(rlv_texture_set_filter(window, texture,
                RLV_TEXTURE_FILTER_ANISOTROPIC_16X) == RLV_ERR_UNAVAILABLE,
           "reject unavailable filter without changing state");
    expect(rlv_texture_get_filter(texture) == RLV_TEXTURE_FILTER_BILINEAR,
           "preserve last valid filter");

    const int64_t target = rlv_render_texture_load(window, 800, 450);
    expect(target > 0, "create logical scene target");
    expect(rlv_window_close(window) == RLV_ERR_RESOURCE_LIVE,
           "reject closing a window with live textures and targets");
    const int64_t outputFilter = cycle % 2 == 0
        ? RLV_TEXTURE_FILTER_POINT : RLV_TEXTURE_FILTER_BILINEAR;
    expect(rlv_render_texture_set_filter(window, target, outputFilter) == RLV_OK &&
               rlv_render_texture_get_filter(target) == outputFilter,
           "select nearest or smooth canvas output");
    expect(rlv_render_texture_set_filter(window, target,
                RLV_TEXTURE_FILTER_TRILINEAR) == RLV_ERR_INVALID_ARGUMENT,
           "reject unsupported render-target filter");

    const int64_t frame = rlv_begin_drawing(window);
    const int64_t scope = rlv_render_target_begin(frame, target);
    expect(scope > 0, "begin target");
    expect(rlv_window_framebuffer_width(window) == 1600 &&
               rlv_window_logical_width(window) == 800,
           "active render target does not replace cached window metrics");
    expect(rlv_texture_draw_pro(frame, texture, 0, 0, 32, 32,
                100, 100, 64, 64, 32, 32, 15, 255, 255, 255, 255) == RLV_OK,
           "draw rotated atlas subregion with pivot");
    expect(rlv_texture_draw_pro(frame, texture, 0, 32, 32, -32,
                100, 100, 64, 64, 0, 0, 0, 255, 255, 255, 255) == RLV_OK,
           "flip atlas subregion with source geometry");
    expect(rlv_texture_draw_pro(frame, texture, 0, 0, 100, 100,
                100, 100, 64, 64, 0, 0, 0, 255, 255, 255, 255) ==
               RLV_ERR_INVALID_ARGUMENT,
           "reject out-of-bounds atlas region");
    expect(rlv_render_texture_set_filter(window, target, outputFilter) ==
               RLV_ERR_INVALID_STATE,
           "reject filter mutation while target is active");
    expect(rlv_render_target_end(scope) == RLV_OK, "end target");
    expect(rlv_render_texture_draw_framebuffer(frame, target,
                0, 0, 800, -450, 0, 150, 1600, 900,
                0, 0, 0, 255, 255, 255, 255) == RLV_OK,
           "present logical scene in letterboxed framebuffer");
    expect(rlv_end_drawing(frame) == RLV_OK, "end frame");
    expect(rlv_render_texture_unload(target) == RLV_OK,
           "unload canvas target");
    expect(rlv_render_texture_get_filter(target) == RLV_ERR_STALE_HANDLE,
           "reject stale canvas target");
    expect(rlv_texture_unload(texture) == RLV_OK,
           "unload texture");
    const int64_t staleFrame = rlv_begin_drawing(window);
    expect(rlv_texture_draw(staleFrame, texture, 0, 0,
                            255, 255, 255, 255) == RLV_ERR_STALE_HANDLE,
           "reject stale texture during a valid frame");
    expect(rlv_end_drawing(staleFrame) == RLV_OK,
           "end stale-texture validation frame");
    expect(rlv_texture_unload(texture) == RLV_ERR_STALE_HANDLE,
           "reject stale texture draw and double unload");
  }
  expect(rlv_buffer_destroy(path) == RLV_OK, "release path buffer");
  expect(rlv_texture_live_count() == 0 &&
             rlv_render_texture_live_count() == 0,
         "leave no live rendering resources");
  expect(rlv_window_close(window) == RLV_OK, "close window");
  if (failures != 0) std::cerr << failures << " WP2 adapter failures\n";
  return failures == 0 ? 0 : 1;
}
