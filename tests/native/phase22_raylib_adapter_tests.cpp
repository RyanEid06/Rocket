#include "rocket_raylib_adapter.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <iostream>
#include <string>
#include <string_view>

#include <raylib.h>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase22 adapter failure: " << message << '\n';
}

bool near(double actual, double expected) {
  return std::abs(actual - expected) <= 0.001;
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  }
  return id;
}

void destroyLayout(int64_t layout) {
  expect(layout > 0, "create text layout");
  if (layout > 0) {
    expect(rlv_text_layout_destroy(layout) == RLV_OK,
           "destroy text layout token");
  }
}

void deterministicTypographyCycle() {
  const int64_t title = textBuffer("Typography test");
  const int64_t window = rlv_window_open(320, 180, title);
  expect(window > 0, "open deterministic window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");

  const int64_t font = rlv_font_default(window);
  expect(font > 0, "select the backend default font");
  const int64_t hello = textBuffer("Hello");
  const int64_t first = rlv_font_measure(
      font, hello, 20.0, 2.0, 1.2, 0.0, 0.0, 0, RLV_TEXT_OVERFLOW_CLIP);
  expect(first > 0, "measure unbounded text");
  expect(near(rlv_text_layout_width(first), 58.0) &&
             near(rlv_text_layout_height(first), 24.0) &&
             near(rlv_text_layout_baseline(first), 16.0) &&
             near(rlv_text_layout_line_height(first), 24.0) &&
             rlv_text_layout_line_count(first) == 1 &&
             rlv_text_layout_clipped(first) == 0 &&
             rlv_text_layout_ellipsized(first) == 0,
         "report deterministic selected-font size, baseline, and bounds");
  destroyLayout(first);

  const int64_t hitsBefore = rlv_font_measurement_cache_hits();
  const int64_t repeated = rlv_font_measure(
      font, hello, 20.0, 2.0, 1.2, 0.0, 0.0, 0, RLV_TEXT_OVERFLOW_CLIP);
  expect(repeated > 0 &&
             rlv_font_measurement_cache_hits() == hitsBefore + 1,
         "reuse an identical cached measurement");
  destroyLayout(repeated);

  const int64_t wrappedText = textBuffer("one two three four");
  const int64_t wrapped = rlv_font_measure(
      font, wrappedText, 20.0, 0.0, 1.0, 50.0, 45.0, 1,
      RLV_TEXT_OVERFLOW_ELLIPSIS);
  expect(wrapped > 0 && rlv_text_layout_line_count(wrapped) == 2 &&
             near(rlv_text_layout_width(wrapped), 50.0) &&
             near(rlv_text_layout_height(wrapped), 40.0) &&
             rlv_text_layout_clipped(wrapped) == 1 &&
             rlv_text_layout_ellipsized(wrapped) == 1,
         "wrap, bound multiline height, and ellipsize the last line");
  destroyLayout(wrapped);

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0, "begin deterministic typography frame");
  expect(rlv_font_draw_layout(
             frame, font, hello, 0.0, 0.0, 100.0, 50.0,
             20.0, 2.0, 1.0,
             RLV_TEXT_ALIGN_CENTER, RLV_TEXT_ALIGN_MIDDLE,
             0, 1, RLV_TEXT_OVERFLOW_CLIP,
             255, 255, 255, 255) == RLV_OK,
         "draw centered and clipped text layout");
  expect(near(rlv_test_text_draw_x(), 21.0) &&
             near(rlv_test_text_draw_y(), 15.0),
         "place text from measured bounds instead of guessed glyph width");
  expect(rlv_font_draw_layout(
             frame, font, hello, 10.0, 40.0, 100.0, 30.0,
             20.0, 2.0, 1.0,
             RLV_TEXT_ALIGN_RIGHT, RLV_TEXT_ALIGN_BASELINE,
             0, 0, RLV_TEXT_OVERFLOW_CLIP,
             255, 255, 255, 255) == RLV_OK,
         "draw right-aligned text from a baseline anchor");
  expect(near(rlv_test_text_draw_x(), 52.0) &&
             near(rlv_test_text_draw_y(), 24.0),
         "apply right and baseline alignment");
  expect(rlv_end_drawing(frame) == RLV_OK, "end typography frame");

  expect(rlv_font_measure(font, hello, -1.0, 0.0, 1.0, 0.0, 0.0,
                          0, RLV_TEXT_OVERFLOW_CLIP) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject invalid style values before native measurement");
  expect(rlv_font_measure(99999, hello, 20.0, 0.0, 1.0, 0.0, 0.0,
                          0, RLV_TEXT_OVERFLOW_CLIP) ==
             RLV_ERR_STALE_HANDLE,
         "reject stale font measurement");

  for (int index = 0; index < 300; ++index) {
    const double size = 8.0 + static_cast<double>(index);
    const int64_t layout = rlv_font_measure(
        font, hello, size, 0.0, 1.0, 0.0, 0.0, 0,
        RLV_TEXT_OVERFLOW_CLIP);
    destroyLayout(layout);
  }
  expect(rlv_font_measurement_cache_capacity() == 256 &&
             rlv_font_measurement_cache_size() <=
                 rlv_font_measurement_cache_capacity(),
         "bound the measurement cache under style churn");

  const int64_t otherFont = rlv_font_default(window);
  const int64_t otherLayout = rlv_font_measure(
      otherFont, hello, 19.0, 0.0, 1.0, 0.0, 0.0, 0,
      RLV_TEXT_OVERFLOW_CLIP);
  destroyLayout(otherLayout);
  expect(rlv_font_invalidate_measurements(font) == RLV_OK &&
             rlv_font_measurement_cache_size() == 1,
         "invalidate only measurements for the selected font");
  expect(rlv_font_unload(otherFont) == RLV_OK &&
             rlv_font_measurement_cache_size() == 0,
         "invalidate an unrelated font cache only when that font unloads");

  const int64_t liveLayout = rlv_font_measure(
      font, hello, 20.0, 0.0, 1.0, 0.0, 0.0, 0,
      RLV_TEXT_OVERFLOW_CLIP);
  expect(liveLayout > 0, "create a font-dependent layout token");
  expect(rlv_font_unload(font) == RLV_ERR_RESOURCE_LIVE,
         "reject font unload while a dependent layout token is live");
  destroyLayout(liveLayout);

  expect(rlv_buffer_destroy(hello) == RLV_OK &&
             rlv_buffer_destroy(wrappedText) == RLV_OK,
         "destroy typography text buffers");
  expect(rlv_font_unload(font) == RLV_OK, "release default font token");
  expect(rlv_text_layout_live_count() == 0 &&
             rlv_font_measurement_cache_size() == 0,
         "release layout tokens and invalidate font cache on unload");
  expect(rlv_window_close(window) == RLV_OK,
         "close deterministic typography window");
}

void nativeTypographyScene(const char* outputPath) {
  SetConfigFlags(FLAG_WINDOW_HIDDEN);
  const int64_t title = textBuffer("WP22 native typography regression");
  const int64_t window = rlv_window_open(320, 180, title);
  expect(window > 0, "open hidden native typography window");
  const int64_t font = rlv_font_default(window);
  const int64_t text = textBuffer("Rocket typography\nmeasures real glyphs");
  const int64_t layout = rlv_font_measure(
      font, text, 24.0, 1.0, 1.25, 280.0, 120.0, 1,
      RLV_TEXT_OVERFLOW_ELLIPSIS);
  expect(layout > 0 && rlv_text_layout_width(layout) > 0.0 &&
             rlv_text_layout_height(layout) > 0.0 &&
             rlv_text_layout_baseline(layout) > 0.0,
         "measure the actual native selected font");
  destroyLayout(layout);
  const int64_t frame = rlv_begin_drawing(window);
  expect(rlv_clear_background(frame, 18, 22, 31, 255) == RLV_OK,
         "clear native typography scene");
  expect(rlv_font_draw_layout(
             frame, font, text, 20.0, 20.0, 280.0, 120.0,
             24.0, 1.0, 1.25,
             RLV_TEXT_ALIGN_CENTER, RLV_TEXT_ALIGN_MIDDLE,
             1, 1, RLV_TEXT_OVERFLOW_ELLIPSIS,
             245, 245, 245, 255) == RLV_OK,
         "draw native wrapped typography scene");
  expect(rlv_end_drawing(frame) == RLV_OK, "end native typography frame");
  const int64_t output = textBuffer(outputPath);
  expect(rlv_window_screenshot(window, output) == RLV_OK,
         "capture native typography scene");
  Image image = LoadImage(outputPath);
  expect(IsImageValid(image) && image.width == 320 && image.height == 180,
         "load captured typography scene");
  if (IsImageValid(image)) UnloadImage(image);
  expect(rlv_buffer_destroy(output) == RLV_OK &&
             rlv_buffer_destroy(text) == RLV_OK &&
             rlv_buffer_destroy(title) == RLV_OK,
         "destroy native typography buffers");
  expect(rlv_font_unload(font) == RLV_OK &&
             rlv_window_close(window) == RLV_OK,
         "clean native typography resources");
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2) {
    nativeTypographyScene(argv[1]);
    std::remove(argv[1]);
  }
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  deterministicTypographyCycle();
  expect(rlv_buffer_live_count() == 0 && rlv_font_live_count() == 0 &&
             rlv_text_layout_live_count() == 0,
         "clean every typography resource");
  if (failures == 0) {
    std::cout << "phase22 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase22 adapter test failure(s)\n";
  return 1;
}
