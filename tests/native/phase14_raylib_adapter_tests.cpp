#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <iostream>
#include <string_view>
#include <type_traits>

static_assert(std::is_same_v<decltype(&rlv_buffer_push),
                             int64_t (*)(int64_t, uint8_t)>);
static_assert(std::is_same_v<decltype(&rlv_apply_callback),
                             int64_t (*)(RlvIntCallback, int64_t)>);

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase14 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value)
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push UTF-8 byte");
  return id;
}

int64_t plusSeven(int64_t value) { return value + 7; }

void windowCycle() {
  const int64_t title = textBuffer("Rocket adapter test");
  const int64_t window = rlv_window_open(640, 360, title);
  expect(window > 0, "open headless window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy borrowed title after open");

  const int64_t path = textBuffer("assets/orbit.svg");
  const int64_t texture = rlv_texture_load(window, path);
  expect(texture > 0, "load simulated texture");
  expect(rlv_buffer_destroy(path) == RLV_OK, "destroy borrowed path after load");
  expect(rlv_window_close(window) == RLV_ERR_RESOURCE_LIVE,
         "reject window shutdown with live texture");

  const int64_t fontPath = textBuffer("assets/test-font.ttf");
  const int64_t font = rlv_font_load(window, fontPath);
  expect(font > 0, "load simulated font");
  expect(rlv_buffer_destroy(fontPath) == RLV_OK, "destroy borrowed font path");

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0, "begin frame");
  expect(rlv_clear_background(frame, 10, 20, 30, 255) == RLV_OK,
         "clear active frame");
  expect(rlv_draw_rectangle(frame, 1, 2, 30, 40, 1, 2, 3, 255) == RLV_OK,
         "draw rectangle");
  expect(rlv_texture_draw(frame, texture, 4, 5, 255, 255, 255, 255) == RLV_OK,
         "draw texture");
  const int64_t label = textBuffer("font");
  expect(rlv_font_draw(frame, font, label, 6, 7, 18.0, 1.0,
                       255, 255, 255, 255) == RLV_OK,
         "draw custom font text");
  expect(rlv_buffer_destroy(label) == RLV_OK, "destroy borrowed label buffer");
  expect(rlv_end_drawing(frame) == RLV_OK, "end frame");
  expect(rlv_end_drawing(frame) == RLV_ERR_STALE_HANDLE,
         "reject reused frame token");

  expect(rlv_texture_unload(texture) == RLV_OK, "unload texture");
  expect(rlv_font_unload(font) == RLV_OK, "unload font");
  expect(rlv_texture_unload(texture) == RLV_ERR_STALE_HANDLE,
         "reject double texture unload");
  expect(rlv_window_close(window) == RLV_OK, "close window after resources");
  expect(rlv_window_close(window) == RLV_ERR_STALE_HANDLE,
         "reject double window close");
}

void audioCycle() {
  const int64_t audio = rlv_audio_open();
  expect(audio > 0 && rlv_audio_ready(audio), "open headless audio");
  for (int index = 0; index < 128; ++index) {
    const int64_t sound = rlv_sound_tone(audio, 220.0 + index, 0.02);
    expect(sound > 0, "create procedural sound");
    expect(rlv_sound_set_volume(sound, 0.5) == RLV_OK, "set sound volume");
    expect(rlv_sound_play(sound) == RLV_OK, "play sound");
    expect(rlv_sound_stop(sound) == RLV_OK, "stop sound");
    expect(rlv_sound_unload(sound) == RLV_OK, "unload sound");
  }
  expect(rlv_sound_live_count() == 0, "no live sounds after stress");
  expect(rlv_audio_close(audio) == RLV_OK, "close audio");
}

}  // namespace

int main() {
  expect(rlv_version_major() == 6 && rlv_version_minor() == 0,
         "adapter is compiled against raylib 6.0");
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable deterministic backend");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  expect(rlv_apply_callback(plusSeven, 35) == 42,
         "invoke synchronous non-storing callback");

  for (int index = 0; index < 64; ++index) windowCycle();
  audioCycle();

  const int64_t missing = textBuffer("assets/missing.png");
  const int64_t title = textBuffer("missing asset");
  const int64_t window = rlv_window_open(320, 200, title);
  expect(rlv_texture_load(window, missing) == RLV_ERR_NOT_FOUND,
         "report missing asset without creating a resource");
  expect(rlv_buffer_destroy(missing) == RLV_OK, "release missing path buffer");
  expect(rlv_buffer_destroy(title) == RLV_OK, "release final title buffer");
  expect(rlv_window_close(window) == RLV_OK, "close final window");
  expect(rlv_buffer_live_count() == 0 && rlv_texture_live_count() == 0 &&
             rlv_font_live_count() == 0 && rlv_sound_live_count() == 0,
         "all native registries are empty");

  return failures == 0 ? 0 : 1;
}
