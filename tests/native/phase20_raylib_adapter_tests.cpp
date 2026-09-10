#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <iostream>
#include <string_view>

namespace {
int failures = 0;
void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase20 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push title byte");
  }
  return id;
}
}

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable deterministic backend");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  const int64_t title = textBuffer("WP20 input");
  const int64_t window = rlv_window_open(320, 200, title);
  expect(window > 0, "open deterministic window");
  expect(rlv_buffer_destroy(title) == RLV_OK, "destroy title buffer");

  expect(rlv_test_set_mouse_state(30, 40, 1, 1, 1) == RLV_OK,
         "set complete pointer state");
  expect(rlv_mouse_down(window, RLV_MOUSE_LEFT) == 1,
         "report held pointer state");
  expect(rlv_mouse_pressed(window, RLV_MOUSE_LEFT) == 1 &&
             rlv_mouse_pressed(window, RLV_MOUSE_LEFT) == 0,
         "pressed is a one-shot edge");
  expect(rlv_mouse_released(window, RLV_MOUSE_LEFT) == 1 &&
             rlv_mouse_released(window, RLV_MOUSE_LEFT) == 0,
         "released is a one-shot edge");
  expect(rlv_mouse_down(window, RLV_MOUSE_LEFT) == 1,
         "edge reads do not clear held state");

  expect(rlv_test_set_mouse_state(31, 41, 0, 0, 0) == RLV_OK,
         "clear complete pointer state");
  expect(rlv_mouse_down(window, RLV_MOUSE_LEFT) == 0,
         "clear held pointer state");
  expect(rlv_test_set_mouse(50, 60, 1) == RLV_OK,
         "legacy pointer scripting remains supported");
  expect(rlv_mouse_pressed(window, RLV_MOUSE_LEFT) == 1,
         "legacy pointer scripting preserves pressed behavior");
  expect(rlv_mouse_down(window, RLV_MOUSE_LEFT) == 0 &&
             rlv_mouse_released(window, RLV_MOUSE_LEFT) == 0,
         "legacy scripting does not invent held or released state");

  expect(rlv_mouse_down(window + 1, RLV_MOUSE_LEFT) == 0 &&
             rlv_mouse_released(window + 1, RLV_MOUSE_LEFT) == 0,
         "invalid windows cannot report pointer activity");
  expect(rlv_window_close(window) == RLV_OK, "close deterministic window");
  return failures == 0 ? 0 : 1;
}
