#ifndef ROCKET_PHASE13_NATIVE_H
#define ROCKET_PHASE13_NATIVE_H
#include <stdint.h>
typedef uint8_t rocket_bool;
#define NATIVE_BIAS 5
#ifndef ROCKET_API
#define ROCKET_API
#endif
#ifdef __cplusplus
extern "C" {
#endif
typedef struct NativeCounter NativeCounter;
typedef struct NativePoint { int64_t x; int64_t y; } NativePoint;
typedef int64_t (*NativeUnary)(int64_t value);
ROCKET_API int64_t phase13_add(int64_t left, int64_t right);
ROCKET_API rocket_bool phase13_not(rocket_bool value);
ROCKET_API double phase13_scale(double value);
ROCKET_API uint8_t phase13_echo_char(uint8_t value);
ROCKET_API int64_t phase13_status(int64_t value);
ROCKET_API NativeCounter* phase13_counter_new(int64_t value);
ROCKET_API int64_t phase13_counter_read(NativeCounter* counter);
ROCKET_API void phase13_counter_destroy(NativeCounter* counter);
ROCKET_API int64_t phase13_counter_live_count(void);
ROCKET_API NativePoint* phase13_point_new(int64_t x, int64_t y);
ROCKET_API int64_t phase13_point_sum(NativePoint* point);
ROCKET_API void phase13_point_destroy(NativePoint* point);
ROCKET_API int64_t phase13_apply(NativeUnary action, int64_t value);
#ifdef __cplusplus
}
#endif
#endif
