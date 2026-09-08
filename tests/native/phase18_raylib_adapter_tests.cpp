#include "rocket_raylib_adapter.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string_view>
#include <type_traits>

#include <raylib.h>

static_assert(std::is_same_v<decltype(&rlv_window_open_quality),
                             int64_t (*)(int64_t, int64_t, int64_t,
                                         rocket_bool, rocket_bool,
                                         rocket_bool)>);
static_assert(std::is_same_v<decltype(&rlv_window_set_fullscreen),
                             int64_t (*)(int64_t, rocket_bool)>);
static_assert(std::is_same_v<decltype(&rlv_window_set_borderless),
                             int64_t (*)(int64_t, rocket_bool)>);
static_assert(std::is_same_v<decltype(&rlv_window_screenshot),
                             int64_t (*)(int64_t, int64_t)>);

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase18 adapter failure: " << message << '\n';
}

bool near(double actual, double expected) {
  return std::abs(actual - expected) <= 0.0001;
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  }
  return id;
}

void deterministicDisplayCycle() {
  const int64_t title = textBuffer("Display quality test");
  expect(rlv_window_open_quality(0, 360, title, 1, 1, 1) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject invalid quality window size");
  const int64_t window =
      rlv_window_open_quality(640, 360, title, 1, 1, 1);
  expect(window > 0, "open configured deterministic window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");
  expect(rlv_window_resizable(window) == 1 &&
             rlv_window_high_dpi(window) == 1 &&
             rlv_window_msaa4x(window) == 1,
         "preserve pre-window quality configuration");
  expect(rlv_window_logical_width(window) == 640 &&
             rlv_window_logical_height(window) == 360 &&
             rlv_window_framebuffer_width(window) == 640 &&
             rlv_window_framebuffer_height(window) == 360 &&
             near(rlv_window_dpi_scale_x(window), 1.0) &&
             near(rlv_window_dpi_scale_y(window), 1.0),
         "report distinct initial logical framebuffer and DPI values");

  const int64_t initialRevision = rlv_window_display_revision(window);
  expect(initialRevision > 0, "start display revision");
  expect(rlv_test_set_display_metrics(800, 450, 1600, 900, 2.0, 2.0,
                                      2, 1) == RLV_OK,
         "simulate DPI monitor transition");
  expect(rlv_window_logical_width(window) == 800 &&
             rlv_window_logical_height(window) == 450 &&
             rlv_window_framebuffer_width(window) == 1600 &&
             rlv_window_framebuffer_height(window) == 900 &&
             near(rlv_window_dpi_scale_x(window), 2.0) &&
             near(rlv_window_dpi_scale_y(window), 2.0),
         "refresh logical framebuffer and DPI values");
  expect(rlv_window_display_revision(window) == initialRevision + 1 &&
             rlv_window_resized(window) == 1,
         "signal mapping invalidation exactly once");
  expect(rlv_window_monitor_count(window) == 2 &&
             rlv_window_current_monitor(window) == 1,
         "report explicit monitor selection");
  expect(rlv_monitor_valid(window, 1) == 1 &&
             rlv_monitor_width(window, 1) > 0 &&
             rlv_monitor_height(window, 1) > 0 &&
             rlv_monitor_physical_width(window, 1) > 0 &&
             rlv_monitor_physical_height(window, 1) > 0 &&
             rlv_monitor_refresh_rate(window, 1) > 0 &&
             rlv_monitor_valid(window, 2) == 0,
         "report reviewed monitor geometry and refresh information");

  expect(rlv_test_set_mouse(125, 50, 0) == RLV_OK,
         "set logical pointer");
  expect(near(rlv_mouse_framebuffer_x(window), 250.0) &&
             near(rlv_mouse_framebuffer_y(window), 100.0),
         "convert logical pointer to physical framebuffer coordinates");

  expect(rlv_window_set_size(window, 1024, 576) == RLV_OK,
         "resize supported window");
  expect(rlv_window_logical_width(window) == 1024 &&
             rlv_window_logical_height(window) == 576,
         "publish resized logical dimensions");
  expect(rlv_window_display_revision(window) == initialRevision + 2,
         "resize advances display revision");
  expect(rlv_window_set_size(window, -1, 576) == RLV_ERR_INVALID_ARGUMENT,
         "reject invalid resize dimensions");
  const int64_t activeFrame = rlv_begin_drawing(window);
  expect(rlv_window_set_size(window, 800, 450) == RLV_ERR_INVALID_STATE &&
             rlv_window_set_fullscreen(window, 1) == RLV_ERR_INVALID_STATE,
         "reject display transitions during an active frame");
  expect(rlv_end_drawing(activeFrame) == RLV_OK,
         "end transition guard frame");
  expect(rlv_window_resized(window) == 0,
         "resize event clears after deterministic frame boundary");
  expect(rlv_window_set_monitor(window, 2) == RLV_ERR_NOT_FOUND,
         "reject missing monitor selection");
  expect(rlv_window_set_monitor(window, 0) == RLV_OK &&
             rlv_window_current_monitor(window) == 0,
         "select an available monitor");

  expect(rlv_test_set_display_capabilities(1, 1, 1, 1, 1) == RLV_OK,
         "enable deterministic display capabilities");
  expect(rlv_window_resize_supported(window) == 1 &&
             rlv_window_fullscreen_supported(window) == 1 &&
             rlv_window_borderless_supported(window) == 1 &&
             rlv_window_monitor_selection_supported(window) == 1 &&
             rlv_window_screenshot_supported(window) == 1,
         "report supported capabilities explicitly");
  expect(rlv_window_set_fullscreen(window, 1) == RLV_OK &&
             rlv_window_fullscreen(window) == 1 &&
             rlv_window_borderless(window) == 0,
         "enter exclusive fullscreen");
  expect(rlv_window_set_borderless(window, 1) == RLV_OK &&
             rlv_window_borderless(window) == 1 &&
             rlv_window_fullscreen(window) == 0,
         "switch atomically to borderless fullscreen");

  const int64_t screenshotPath = textBuffer("wp18-display.png");
  expect(rlv_window_screenshot(window, screenshotPath) == RLV_OK &&
             rlv_screenshot_count() == 1,
         "take checked screenshot");
  const int64_t invalidPath = textBuffer("wp18-display.jpg");
  expect(rlv_window_screenshot(window, invalidPath) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject unsupported screenshot extension");
  expect(rlv_buffer_destroy(invalidPath) == RLV_OK,
         "destroy invalid screenshot path");
  expect(rlv_buffer_destroy(screenshotPath) == RLV_OK,
         "destroy screenshot path");

  expect(rlv_test_set_display_capabilities(0, 0, 0, 0, 0) == RLV_OK,
         "disable deterministic display capabilities");
  expect(rlv_window_set_size(window, 800, 450) == RLV_ERR_UNAVAILABLE &&
             rlv_window_set_fullscreen(window, 1) == RLV_ERR_UNAVAILABLE &&
             rlv_window_set_borderless(window, 1) == RLV_ERR_UNAVAILABLE &&
             rlv_window_set_monitor(window, 1) == RLV_ERR_UNAVAILABLE,
         "return defined transition errors when unsupported");
  const int64_t unsupportedPath = textBuffer("wp18-unsupported.png");
  expect(rlv_window_screenshot(window, unsupportedPath) ==
             RLV_ERR_UNAVAILABLE,
         "return defined screenshot error when unsupported");
  expect(rlv_buffer_destroy(unsupportedPath) == RLV_OK,
         "destroy unsupported screenshot path");

  expect(rlv_window_close(window) == RLV_OK,
         "close deterministic quality window");
}

void nativeDisplayCycle() {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  const int64_t title = textBuffer("WP18 native display regression");
  const int64_t window =
      rlv_window_open_quality(96, 64, title, 1, 1, 1);
  expect(window > 0, "open hidden native quality window");
  expect(IsWindowState(FLAG_WINDOW_RESIZABLE) &&
             IsWindowState(FLAG_WINDOW_HIGHDPI) &&
             IsWindowState(FLAG_MSAA_4X_HINT),
         "apply native pre-window quality flags");
  expect(rlv_window_logical_width(window) > 0 &&
             rlv_window_logical_height(window) > 0 &&
             rlv_window_framebuffer_width(window) > 0 &&
             rlv_window_framebuffer_height(window) > 0 &&
             rlv_window_dpi_scale_x(window) > 0.0 &&
             rlv_window_dpi_scale_y(window) > 0.0,
         "read live native display metrics");
  expect(rlv_window_monitor_count(window) > 0 &&
             rlv_window_current_monitor(window) >= 0,
         "read live native monitor selection");

  expect(rlv_window_resize_supported(window) == 1 &&
             rlv_window_fullscreen_supported(window) == 1 &&
             rlv_window_borderless_supported(window) == 1 &&
             rlv_window_monitor_selection_supported(window) == 1,
         "report native desktop transition capabilities");
  const int64_t resizeStatus = rlv_window_set_size(window, 320, 240);
  const int64_t resizedWidth = rlv_window_logical_width(window);
  const int64_t resizedHeight = rlv_window_logical_height(window);
  if (resizeStatus != RLV_OK || resizedWidth != 320 || resizedHeight != 240) {
    std::cerr << "native resize status=" << resizeStatus << " logical="
              << resizedWidth << 'x' << resizedHeight << " framebuffer="
              << rlv_window_framebuffer_width(window) << 'x'
              << rlv_window_framebuffer_height(window) << '\n';
  }
  expect(resizeStatus == RLV_OK && resizedWidth == 320 && resizedHeight == 240,
         "apply and verify native window resize");
  const int64_t originalMonitor = rlv_window_current_monitor(window);
  if (rlv_window_monitor_count(window) > 1) {
    const int64_t alternateMonitor = originalMonitor == 0 ? 1 : 0;
    expect(rlv_window_set_monitor(window, alternateMonitor) == RLV_OK &&
               rlv_window_current_monitor(window) == alternateMonitor &&
               rlv_window_set_monitor(window, originalMonitor) == RLV_OK,
           "apply and restore native monitor transition");
  } else {
    expect(rlv_window_set_monitor(window, originalMonitor) == RLV_OK,
           "accept native current-monitor selection");
  }
  expect(rlv_window_set_fullscreen(window, 1) == RLV_OK &&
             rlv_window_fullscreen(window) == 1 &&
             rlv_window_borderless(window) == 0,
         "enter native exclusive fullscreen");
  expect(rlv_window_set_fullscreen(window, 0) == RLV_OK &&
             rlv_window_fullscreen(window) == 0,
         "leave native exclusive fullscreen");
  expect(rlv_window_set_borderless(window, 1) == RLV_OK &&
             rlv_window_borderless(window) == 1 &&
             rlv_window_fullscreen(window) == 0,
         "enter native borderless fullscreen");
  expect(rlv_window_set_borderless(window, 0) == RLV_OK &&
             rlv_window_borderless(window) == 0,
         "leave native borderless fullscreen");

  const int64_t framebufferWidth = rlv_window_framebuffer_width(window);
  const int64_t framebufferHeight = rlv_window_framebuffer_height(window);
  const int64_t displayRevision = rlv_window_display_revision(window);
  const int64_t target = rlv_render_texture_load(window, 24, 16);
  const int64_t frame = rlv_begin_drawing(window);
  const int64_t targetScope = rlv_render_target_begin(frame, target);
  expect(target > 0 && frame > 0 && targetScope > 0,
         "begin native render target for framebuffer query regression");
  expect(rlv_window_framebuffer_width(window) == framebufferWidth &&
             rlv_window_framebuffer_height(window) == framebufferHeight &&
             rlv_window_display_revision(window) == displayRevision,
         "keep window metrics stable while a render target is active");
  expect(rlv_render_target_end(targetScope) == RLV_OK &&
             rlv_end_drawing(frame) == RLV_OK &&
             rlv_render_texture_unload(target) == RLV_OK,
         "end native framebuffer query regression scope");

  const int64_t screenshotPath = textBuffer("wp18-native-window.png");
  expect(rlv_window_screenshot(window, screenshotPath) == RLV_OK,
         "capture live native framebuffer");
  Image screenshot = LoadImage("wp18-native-window.png");
  expect(IsImageValid(screenshot), "load native screenshot artifact");
  if (IsImageValid(screenshot)) {
    expect(screenshot.width == rlv_window_framebuffer_width(window) &&
               screenshot.height == rlv_window_framebuffer_height(window),
           "screenshot uses physical framebuffer dimensions");
    UnloadImage(screenshot);
  }
  std::remove("wp18-native-window.png");
  expect(rlv_window_close(window) == RLV_OK, "close hidden native window");
  expect(rlv_buffer_destroy(title) == RLV_OK &&
             rlv_buffer_destroy(screenshotPath) == RLV_OK,
         "destroy native display buffers");

  const int64_t reopenTitle = textBuffer("WP18 native reopen regression");
  expect(rlv_window_open(80, 60, reopenTitle) == RLV_ERR_UNAVAILABLE,
         "reject removing process-lifetime native quality flags");
  const int64_t reopened =
      rlv_window_open_quality(80, 60, reopenTitle, 1, 1, 1);
  expect(reopened > 0, "reopen with compatible native quality flags");
  expect(IsWindowState(FLAG_WINDOW_RESIZABLE) &&
             IsWindowState(FLAG_WINDOW_HIGHDPI) &&
             IsWindowState(FLAG_MSAA_4X_HINT),
         "preserve compatible native quality flags on reopen");
  expect(rlv_window_close(reopened) == RLV_OK,
         "close compatibly reopened native window");
  expect(rlv_buffer_destroy(reopenTitle) == RLV_OK,
         "destroy native reopen title");
}

}  // namespace

int main(int argc, char**) {
  if (argc == 2) nativeDisplayCycle();
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  deterministicDisplayCycle();
  expect(rlv_buffer_live_count() == 0, "clean all display buffers");
  if (failures == 0) {
    std::cout << "phase18 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase18 adapter test failure(s)\n";
  return 1;
}
