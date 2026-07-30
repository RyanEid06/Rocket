#ifndef ROCKET_PHASE13_NATIVE_H
#define ROCKET_PHASE13_NATIVE_H

#include <stdint.h>

typedef uint8_t rocket_bool;
#define NATIVE_BIAS 5
typedef struct NativeCounter NativeCounter;
typedef struct NativePoint { int64_t x; int64_t y; } NativePoint;
typedef int64_t (*NativeUnary)(int64_t value);

#ifdef __cplusplus
extern "C" {
#endif

int64_t phase13_add(int64_t left, int64_t right);
rocket_bool phase13_not(rocket_bool value);
double phase13_scale(double value);
uint8_t phase13_echo_char(uint8_t value);
int64_t phase13_status(int64_t value);
NativeCounter* phase13_counter_new(int64_t value);
int64_t phase13_counter_read(NativeCounter* counter);
void phase13_counter_destroy(NativeCounter* counter);
int64_t phase13_counter_live_count(void);
NativePoint* phase13_point_new(int64_t x, int64_t y);
int64_t phase13_point_sum(NativePoint* point);
void phase13_point_destroy(NativePoint* point);
int64_t phase13_apply(NativeUnary action, int64_t value);

#ifdef __cplusplus
}
#endif

#endif
