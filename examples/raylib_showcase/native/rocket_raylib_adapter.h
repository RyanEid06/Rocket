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

#define RLV_TEXTURE_FILTER_POINT 0
#define RLV_TEXTURE_FILTER_BILINEAR 1
#define RLV_TEXTURE_FILTER_TRILINEAR 2
#define RLV_TEXTURE_FILTER_ANISOTROPIC_4X 3
#define RLV_TEXTURE_FILTER_ANISOTROPIC_8X 4
#define RLV_TEXTURE_FILTER_ANISOTROPIC_16X 5

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
ROCKET_API int64_t rlv_point_buffer_create(void);
ROCKET_API int64_t rlv_point_buffer_push(int64_t buffer_id, double x, double y);
ROCKET_API int64_t rlv_point_buffer_destroy(int64_t buffer_id);
ROCKET_API int64_t rlv_point_buffer_live_count(void);

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
ROCKET_API int64_t rlv_draw_rectangle_outline(int64_t frame_id, double x, double y, double width, double height, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_rounded_rectangle(int64_t frame_id, double x, double y, double width, double height, double roundness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_rounded_rectangle_outline(int64_t frame_id, double x, double y, double width, double height, double roundness, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_rectangle_gradient_vertical(int64_t frame_id, double x, double y, double width, double height, int64_t top_red, int64_t top_green, int64_t top_blue, int64_t top_alpha, int64_t bottom_red, int64_t bottom_green, int64_t bottom_blue, int64_t bottom_alpha);
ROCKET_API int64_t rlv_draw_rectangle_gradient_horizontal(int64_t frame_id, double x, double y, double width, double height, int64_t left_red, int64_t left_green, int64_t left_blue, int64_t left_alpha, int64_t right_red, int64_t right_green, int64_t right_blue, int64_t right_alpha);
ROCKET_API int64_t rlv_draw_rectangle_gradient_four(int64_t frame_id, double x, double y, double width, double height, int64_t top_left_red, int64_t top_left_green, int64_t top_left_blue, int64_t top_left_alpha, int64_t bottom_left_red, int64_t bottom_left_green, int64_t bottom_left_blue, int64_t bottom_left_alpha, int64_t bottom_right_red, int64_t bottom_right_green, int64_t bottom_right_blue, int64_t bottom_right_alpha, int64_t top_right_red, int64_t top_right_green, int64_t top_right_blue, int64_t top_right_alpha);
ROCKET_API int64_t rlv_draw_circle_outline(int64_t frame_id, double x, double y, double radius, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_ellipse(int64_t frame_id, double x, double y, double radius_x, double radius_y, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_ellipse_outline(int64_t frame_id, double x, double y, double radius_x, double radius_y, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_ring(int64_t frame_id, double x, double y, double inner_radius, double outer_radius, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_ring_outline(int64_t frame_id, double x, double y, double inner_radius, double outer_radius, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_ring_sector(int64_t frame_id, double x, double y, double inner_radius, double outer_radius, double start_angle, double end_angle, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_circle_sector(int64_t frame_id, double x, double y, double radius, double start_angle, double end_angle, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_circle_sector_outline(int64_t frame_id, double x, double y, double radius, double start_angle, double end_angle, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_circle_gradient(int64_t frame_id, double x, double y, double radius, int64_t inner_red, int64_t inner_green, int64_t inner_blue, int64_t inner_alpha, int64_t outer_red, int64_t outer_green, int64_t outer_blue, int64_t outer_alpha);
ROCKET_API int64_t rlv_draw_line(int64_t frame_id, double start_x, double start_y, double end_x, double end_y, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_thick_line(int64_t frame_id, double start_x, double start_y, double end_x, double end_y, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_triangle(int64_t frame_id, double first_x, double first_y, double second_x, double second_y, double third_x, double third_y, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_triangle_outline(int64_t frame_id, double first_x, double first_y, double second_x, double second_y, double third_x, double third_y, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_polygon(int64_t frame_id, double x, double y, int64_t sides, double radius, double rotation, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_polygon_outline(int64_t frame_id, double x, double y, int64_t sides, double radius, double rotation, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_bezier_line(int64_t frame_id, double start_x, double start_y, double end_x, double end_y, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_bezier_quadratic(int64_t frame_id, int64_t point_buffer_id, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_draw_bezier_cubic(int64_t frame_id, int64_t point_buffer_id, double thickness, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_geometry_call_count(void);
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
ROCKET_API int64_t rlv_texture_draw_pro(int64_t frame_id, int64_t texture_id, double source_x, double source_y, double source_width, double source_height, double dest_x, double dest_y, double dest_width, double dest_height, double origin_x, double origin_y, double rotation, int64_t red, int64_t green, int64_t blue, int64_t alpha);
ROCKET_API int64_t rlv_texture_set_filter(int64_t window_id, int64_t texture_id, int64_t filter_mode);
ROCKET_API int64_t rlv_texture_get_filter(int64_t texture_id);
ROCKET_API rocket_bool rlv_texture_filter_supported(int64_t filter_mode);
ROCKET_API double rlv_texture_max_anisotropy(void);
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
ROCKET_API int64_t rlv_test_set_anisotropy(int64_t level);

#ifdef __cplusplus
}
#endif

#endif
