#include "phase13_native.h"

struct NativeCounter { int64_t value; };

namespace {
int64_t counter_live_count = 0;
}

extern "C" int64_t phase13_add(int64_t left, int64_t right) {
  return left + right;
}

extern "C" rocket_bool phase13_not(rocket_bool value) {
  return value == 0 ? 1 : 0;
}

extern "C" double phase13_scale(double value) { return value * 2.0; }

extern "C" uint8_t phase13_echo_char(uint8_t value) { return value; }

extern "C" int64_t phase13_status(int64_t value) { return value >= 0 ? 0 : -7; }

extern "C" NativeCounter* phase13_counter_new(int64_t value) {
  ++counter_live_count;
  return new NativeCounter{value};
}

extern "C" int64_t phase13_counter_read(NativeCounter* counter) {
  return counter ? counter->value : -1;
}

extern "C" void phase13_counter_destroy(NativeCounter* counter) {
  if (counter) --counter_live_count;
  delete counter;
}

extern "C" int64_t phase13_counter_live_count(void) { return counter_live_count; }

extern "C" NativePoint* phase13_point_new(int64_t x, int64_t y) {
  return new NativePoint{x, y};
}

extern "C" int64_t phase13_point_sum(NativePoint* point) {
  return point ? point->x + point->y : -1;
}

extern "C" void phase13_point_destroy(NativePoint* point) {
  delete point;
}

extern "C" int64_t phase13_apply(NativeUnary action, int64_t value) {
  return action ? action(value) : -1;
}
