#include "../../src/raylib/rocket_raylib_adapter.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string_view>
#include <thread>

namespace {
int failures = 0;
void expect(bool condition, std::string_view label) {
  if (!condition) {
    ++failures;
    std::cerr << "WP4 native null-backend EOF failure: " << label << '\n';
  }
}
int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value)
    expect(rlv_buffer_push(id, byte) == RLV_OK, "buffer byte");
  return id;
}
}  // namespace

int main(int argc, char** argv) {
  if (argc != 2) return 2;
  const char* backend = std::getenv("ROCKET_AUDIO_NULL_BACKEND");
  expect(backend && std::string_view(backend) == "1",
         "null backend must be explicitly selected");
  std::cout << "WP4 native playback using opt-in null audio backend\n";
  const int64_t audio = rlv_audio_open();
  expect(audio > 0 && rlv_audio_ready(audio) == 1, "open null audio device");
  if (audio <= 0) return 1;
  const int64_t path = textBuffer(argv[1]);
  const int64_t music = rlv_music_load(audio, path);
  const int64_t sound = rlv_sound_load(audio, path);
  expect(music > 0, "load short WAV stream");
  expect(sound > 0, "load short sound on null backend");
  if (music <= 0 || sound <= 0) return 1;
  expect(rlv_audio_close(audio) == RLV_ERR_RESOURCE_LIVE,
         "live sound and music retain device ownership");
  expect(rlv_sound_play(sound) == RLV_OK, "play sound on null backend");
  expect(rlv_sound_stop(sound) == RLV_OK, "stop sound on null backend");
  expect(rlv_music_set_looping(music, 0) == RLV_OK, "disable looping");
  expect(rlv_music_play(music) == RLV_OK, "play short stream");
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  bool stopped = false;
  while (std::chrono::steady_clock::now() < deadline) {
    const int64_t playing = rlv_music_playing(music);
    if (playing == 0) {
      stopped = true;
      break;
    }
    expect(playing == 1, "valid playback query");
    expect(rlv_music_update(music) == RLV_OK, "update before EOF");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  expect(stopped, "short native stream reaches natural EOF");
  if (stopped) {
    expect(rlv_music_playing(music) == 0, "EOF reports stopped");
    expect(rlv_music_update(music) == RLV_ERR_INVALID_STATE, "update after EOF");
    expect(rlv_music_pause(music) == RLV_ERR_INVALID_STATE, "pause after EOF");
    expect(rlv_music_resume(music) == RLV_ERR_INVALID_STATE, "resume after EOF");
  }
  expect(rlv_music_set_looping(music, 1) == RLV_OK &&
             rlv_music_play(music) == RLV_OK, "replay after EOF with looping");
  for (int tick = 0; tick < 40; ++tick) {
    expect(rlv_music_update(music) == RLV_OK &&
               rlv_music_playing(music) == 1,
           "looping stream stays active past short-asset duration");
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  expect(rlv_sound_unload(sound) == RLV_OK, "unload null-backend sound");
  expect(rlv_music_unload(music) == RLV_OK, "unload replayed stream");
  expect(rlv_sound_live_count() == 0 && rlv_music_live_count() == 0,
         "no live sound or stream");
  expect(rlv_buffer_destroy(path) == RLV_OK, "release path buffer");
  expect(rlv_audio_close(audio) == RLV_OK, "close null audio device");
  return failures == 0 ? 0 : 1;
}
