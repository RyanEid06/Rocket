#include "../../src/raylib/rocket_raylib_adapter.h"

#include <iostream>
#include <string_view>

namespace {
int failures = 0;
void expect(bool condition, std::string_view label) {
  if (!condition) {
    ++failures;
    std::cerr << "WP4 audio failure: " << label << '\n';
  }
}
int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value)
    expect(rlv_buffer_push(id, byte) == RLV_OK, "buffer byte");
  return id;
}
}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK && rlv_test_reset() == RLV_OK,
         "start headless adapter");
  const int64_t effectPath = textBuffer("effect.wav");
  const int64_t musicPath = textBuffer("music.wav");
  const int64_t missingPath = textBuffer("missing.wav");
  for (int cycle = 0; cycle < 32; ++cycle) {
    const int64_t audio = rlv_audio_open();
    expect(audio > 0 && rlv_audio_ready(audio) == 1, "open audio");
    expect(rlv_audio_open() == RLV_ERR_INVALID_STATE, "reject second device");
    expect(rlv_music_load(audio, missingPath) == RLV_ERR_NOT_FOUND &&
           rlv_sound_load(audio, missingPath) == RLV_ERR_NOT_FOUND,
           "missing audio fails without crashing");
    const int64_t music = rlv_music_load(audio, musicPath);
    const int64_t click = rlv_sound_load(audio, effectPath);
    const int64_t chip = rlv_sound_tone(audio, 540.0, 0.03);
    expect(music > 0 && click > 0 && chip > 0, "load music and effects");
    expect(rlv_audio_close(audio) == RLV_ERR_RESOURCE_LIVE,
           "reject close with live resources");
    expect(rlv_music_update(music) == RLV_ERR_INVALID_STATE &&
           rlv_music_pause(music) == RLV_ERR_INVALID_STATE &&
           rlv_music_resume(music) == RLV_ERR_INVALID_STATE,
           "reject invalid music order");
    expect(rlv_music_set_volume(music, 0.35) == RLV_OK &&
           rlv_music_set_looping(music, 1) == RLV_OK &&
           rlv_sound_set_volume(click, 0.6) == RLV_OK &&
           rlv_sound_set_pitch(chip, 1.2) == RLV_OK,
           "configure music and effects");
    expect(rlv_music_set_volume(music, 2.0) == RLV_ERR_INVALID_ARGUMENT &&
           rlv_sound_set_pitch(click, 0.0) == RLV_ERR_INVALID_ARGUMENT,
           "reject bad audio values");
    expect(rlv_music_play(music) == RLV_OK &&
           rlv_sound_play(click) == RLV_OK && rlv_sound_play(click) == RLV_OK &&
           rlv_sound_play(chip) == RLV_OK &&
           rlv_music_playing(music) == 1 && rlv_sound_playing(click) == 1 &&
           rlv_sound_playing(chip) == 1, "music and two effects overlap");
    for (int tick = 0; tick < 256; ++tick)
      expect(rlv_music_update(music) == RLV_OK, "update streamed music");
    expect(rlv_music_pause(music) == RLV_OK &&
           rlv_music_playing(music) == 0 &&
           rlv_music_update(music) == RLV_ERR_INVALID_STATE &&
           rlv_music_pause(music) == RLV_ERR_INVALID_STATE,
           "pause rejects updates and double pause");
    expect(rlv_music_resume(music) == RLV_OK &&
           rlv_music_playing(music) == 1 &&
           rlv_music_resume(music) == RLV_ERR_INVALID_STATE,
           "resume exactly once");
    expect(rlv_sound_stop(click) == RLV_OK &&
           rlv_sound_playing(click) == 0 &&
           rlv_sound_playing(chip) == 1 &&
           rlv_music_playing(music) == 1, "independent effects and music");
    expect(rlv_music_stop(music) == RLV_OK &&
           rlv_music_playing(music) == 0 &&
           rlv_music_update(music) == RLV_ERR_INVALID_STATE,
           "stop stream");
    expect(rlv_music_unload(music) == RLV_OK &&
           rlv_music_unload(music) == RLV_ERR_STALE_HANDLE &&
           rlv_music_play(music) == RLV_ERR_STALE_HANDLE &&
           rlv_music_playing(music) == RLV_ERR_STALE_HANDLE &&
           rlv_sound_unload(click) == RLV_OK &&
           rlv_sound_play(click) == RLV_ERR_STALE_HANDLE &&
           rlv_sound_playing(click) == RLV_ERR_STALE_HANDLE &&
           rlv_sound_unload(chip) == RLV_OK, "unload invalidates handles");
    expect(rlv_audio_close(audio) == RLV_OK &&
           rlv_audio_ready(audio) == RLV_ERR_STALE_HANDLE &&
           rlv_sound_load(audio, effectPath) == RLV_ERR_STALE_HANDLE &&
           rlv_music_load(audio, musicPath) == RLV_ERR_STALE_HANDLE &&
           rlv_audio_close(audio) == RLV_ERR_STALE_HANDLE,
           "close invalidates device");
  }
  expect(rlv_sound_live_count() == 0 && rlv_music_live_count() == 0,
         "no live audio resources");
  for (int64_t id : {effectPath, musicPath, missingPath})
    expect(rlv_buffer_destroy(id) == RLV_OK, "release buffer");
  return failures == 0 ? 0 : 1;
}
