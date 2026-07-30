#ifndef ROCKET_RAYLIB_ADAPTER_H
#define ROCKET_RAYLIB_ADAPTER_H

#include <stdint.h>

typedef uint8_t rocket_bool;

#define RLV_OK 0
#define RLV_ERR_INVALID_ARGUMENT -1
#define RLV_ERR_INVALID_STATE -2
#define RLV_ERR_NOT_FOUND -3
#define RLV_ERR_RESOURCE_LIVE -4
#define RLV_ERR_UNAVAILABLE -5
#define RLV_ERR_STALE_HANDLE -6

#define RLV_KEY_SPACE 32
#define RLV_KEY_ESCAPE 256
#define RLV_KEY_RIGHT 262
#define RLV_KEY_LEFT 263
#define RLV_KEY_DOWN 264
#define RLV_KEY_UP 265
#define RLV_MOUSE_LEFT 0

#ifndef ROCKET_API
#define ROCKET_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef int64_t (*RlvIntCallback)(int64_t value);

ROCKET_API int64_t rlv_version_major(void);
ROCKET_API int64_t rlv_version_minor(void);
ROCKET_API int64_t rlv_enable_test_mode(rocket_bool enabled);
ROCKET_API int64_t rlv_test_reset(void);

ROCKET_API int64_t rlv_buffer_create(void);
ROCKET_API int64_t rlv_buffer_push(int64_t buffer_id, uint8_t byte_value);
ROCKET_API int64_t rlv_buffer_destroy(int64_t buffer_id);
ROCKET_API int64_t rlv_buffer_live_count(void);

ROCKET_API int64_t rlv_window_open(int64_t width, int64_t height, int64_t title_buffer_id);
ROCKET_API int64_t rlv_window_close(int64_t window_id);
ROCKET_API rocket_bool rlv_window_ready(int64_t window_id);
ROCKET_API rocket_bool rlv_window_should_close(int64_t window_id);
ROCKET_API int64_t rlv_set_target_fps(int64_t window_id, int64_t fps);
ROCKET_API double rlv_frame_time(int64_t window_id);
ROCKET_API double rlv_time(int64_t window_id);

ROCKET_API int64_t rlv_begin_drawing(int64_t window_id);
ROCKET_API int64_t rlv_end_drawing(int64_t frame_id);
ROCKET_API int64_t rlv_clear_background(int64_t frame_id, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_rectangle(int64_t frame_id, int64_t x, int64_t y, int64_t width, int64_t height, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_circle(int64_t frame_id, int64_t x, int64_t y, double radius, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_text(int64_t frame_id, int64_t text_buffer_id, int64_t x, int64_t y, int64_t size, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_count(void);

ROCKET_API rocket_bool rlv_key_pressed(int64_t window_id, int64_t key);
ROCKET_API rocket_bool rlv_key_down(int64_t window_id, int64_t key);
ROCKET_API rocket_bool rlv_mouse_pressed(int64_t window_id, int64_t button);
ROCKET_API int64_t rlv_mouse_x(int64_t window_id);
ROCKET_API int64_t rlv_mouse_y(int64_t window_id);

ROCKET_API int64_t rlv_texture_load(int64_t window_id, int64_t path_buffer_id);
ROCKET_API int64_t rlv_texture_width(int64_t texture_id);
ROCKET_API int64_t rlv_texture_height(int64_t texture_id);
ROCKET_API int64_t rlv_texture_draw(int64_t frame_id, int64_t texture_id, int64_t x, int64_t y, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_texture_draw_scaled(int64_t frame_id, int64_t texture_id, int64_t x, int64_t y, double scale, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_texture_unload(int64_t texture_id);
ROCKET_API int64_t rlv_texture_live_count(void);

ROCKET_API int64_t rlv_font_load(int64_t window_id, int64_t path_buffer_id);
ROCKET_API int64_t rlv_font_draw(int64_t frame_id, int64_t font_id, int64_t text_buffer_id, int64_t x, int64_t y, double size, double spacing, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_font_unload(int64_t font_id);
ROCKET_API int64_t rlv_font_live_count(void);

ROCKET_API int64_t rlv_audio_open(void);
ROCKET_API int64_t rlv_audio_close(int64_t audio_id);
ROCKET_API rocket_bool rlv_audio_ready(int64_t audio_id);
ROCKET_API int64_t rlv_sound_load(int64_t audio_id, int64_t path_buffer_id);
ROCKET_API int64_t rlv_sound_tone(int64_t audio_id, double frequency, double seconds);
ROCKET_API int64_t rlv_sound_play(int64_t sound_id);
ROCKET_API int64_t rlv_sound_stop(int64_t sound_id);
ROCKET_API int64_t rlv_sound_set_volume(int64_t sound_id, double volume);
ROCKET_API int64_t rlv_sound_unload(int64_t sound_id);
ROCKET_API int64_t rlv_sound_live_count(void);

ROCKET_API int64_t rlv_apply_callback(RlvIntCallback callback, int64_t value);

ROCKET_API int64_t rlv_test_set_key(int64_t key, rocket_bool pressed, rocket_bool down);
ROCKET_API int64_t rlv_test_set_mouse(int64_t x, int64_t y, rocket_bool pressed);
ROCKET_API int64_t rlv_test_request_close(rocket_bool requested);

#ifdef __cplusplus
}
#endif

#endif
