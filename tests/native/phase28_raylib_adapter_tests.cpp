#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase28 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push path/name byte");
  }
  return id;
}

struct Fixture {
  std::filesystem::path root;
  std::filesystem::path outside;
  bool symlinkReady = false;

  Fixture() {
    root = std::filesystem::current_path() / "out" /
           "rocket-wp28-relocated-package";
    outside = std::filesystem::current_path() / "out" /
              "rocket-wp28-outside-package";
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::remove_all(outside, error);
    std::filesystem::create_directories(root / "assets", error);
    std::filesystem::create_directories(outside, error);
    std::ofstream(root / "assets" / "sprite.png") << "texture";
    std::ofstream(root / "assets" / "font.ttf") << "font";
    std::ofstream(root / "assets" / "sound.wav") << "sound";
    std::ofstream(root / "assets" / "music.ogg") << "music";
    std::ofstream(root / "assets" / "effect.fs")
        << "uniform float intensity;";
    std::ofstream(outside / "escaped.png") << "outside texture";
    std::filesystem::create_directory_symlink(outside, root / "assets" / "link",
                                              error);
    symlinkReady = !error;
  }

  ~Fixture() {
    std::error_code error;
    std::filesystem::remove_all(root, error);
    std::filesystem::remove_all(outside, error);
  }
};

void deterministicAssetStoreCycle() {
  Fixture fixture;
  const int64_t title = textBuffer("WP28 typed assets");
  const int64_t window = rlv_window_open(320, 180, title);
  const int64_t audio = rlv_audio_open();
  const int64_t root = textBuffer(fixture.root.generic_string());
  const int64_t store = rlv_asset_store_create(window, audio, root);
  expect(window > 0 && audio > 0 && store > 0,
         "open window, audio, and relocated asset store");
  expect(rlv_buffer_destroy(title) == RLV_OK &&
             rlv_buffer_destroy(root) == RLV_OK,
         "release setup buffers");
  expect(rlv_asset_store_live_count() == 1,
         "track one live asset store");
  expect(rlv_window_close(window) == RLV_ERR_RESOURCE_LIVE &&
             rlv_audio_close(audio) == RLV_ERR_RESOURCE_LIVE,
         "store lifetime holds its window and audio dependencies");

  const int64_t heroName = textBuffer("hero");
  const int64_t aliasName = textBuffer("hero-alias");
  const int64_t fontName = textBuffer("body");
  const int64_t soundName = textBuffer("click");
  const int64_t musicName = textBuffer("theme");
  const int64_t shaderName = textBuffer("glow");
  const int64_t missingName = textBuffer("missing-name");
  const int64_t emptyName = textBuffer("");
  const int64_t texturePath = textBuffer("assets/sprite.png");
  const int64_t fontPath = textBuffer("assets/font.ttf");
  const int64_t soundPath = textBuffer("assets/sound.wav");
  const int64_t musicPath = textBuffer("assets/music.ogg");
  const int64_t shaderPath = textBuffer("assets/effect.fs");
  const int64_t emptyPath = textBuffer("");
  const int64_t missingPath = textBuffer("assets/missing.png");
  const int64_t escapePath = textBuffer("../outside.png");
  const int64_t absolutePath = textBuffer(
      (fixture.root.parent_path() / "outside.png").generic_string());
  const int64_t symlinkPath = textBuffer("assets/link/escaped.png");

  const int64_t hero =
      rlv_asset_texture_load(store, heroName, texturePath);
  const int64_t alias =
      rlv_asset_texture_load(store, aliasName, texturePath);
  const int64_t font = rlv_asset_font_load(store, fontName, fontPath);
  const int64_t sound = rlv_asset_sound_load(store, soundName, soundPath);
  const int64_t music = rlv_asset_music_load(store, musicName, musicPath);
  const int64_t shader = rlv_asset_shader_load(
      store, shaderName, emptyPath, shaderPath);
  expect(hero > 0 && alias > 0 && font > 0 && sound > 0 && music > 0 &&
             shader > 0,
         "load every required typed asset");
  expect(rlv_asset_store_asset_count(store) == 6 &&
             rlv_asset_store_physical_count(store) == 5,
         "cache one physical texture behind two logical names");
  const int64_t heroTexture = rlv_asset_texture_borrow(hero);
  const int64_t aliasTexture = rlv_asset_texture_borrow(alias);
  expect(heroTexture > 0 && heroTexture == aliasTexture,
         "borrow the same cached texture through typed references");
  expect(rlv_asset_texture_lookup(store, heroName) == hero &&
             rlv_asset_font_lookup(store, fontName) == font &&
             rlv_asset_sound_lookup(store, soundName) == sound &&
             rlv_asset_music_lookup(store, musicName) == music &&
             rlv_asset_shader_lookup(store, shaderName) == shader,
         "typed lookup returns stable references");
  expect(rlv_asset_texture_lookup(store, fontName) == RLV_ERR_ASSET_TYPE &&
             rlv_asset_texture_borrow(font) == RLV_ERR_ASSET_TYPE,
         "reject wrong-type lookup and borrowing");
  expect(rlv_asset_texture_lookup(store, missingName) == RLV_ERR_NOT_FOUND,
         "report a missing logical asset");
  expect(rlv_asset_texture_load(store, heroName, texturePath) ==
             RLV_ERR_DUPLICATE_ASSET &&
             rlv_asset_store_asset_count(store) == 6 &&
             rlv_asset_store_physical_count(store) == 5,
         "reject duplicate names without changing the cache");
  expect(rlv_asset_texture_load(store, emptyName, texturePath) ==
             RLV_ERR_INVALID_ARGUMENT,
         "reject an empty logical name");
  expect(rlv_asset_texture_load(store, missingName, missingPath) ==
             RLV_ERR_NOT_FOUND,
         "report a missing physical resource");
  expect(rlv_asset_texture_load(store, missingName, escapePath) ==
             RLV_ERR_PATH_ESCAPE &&
             rlv_asset_texture_load(store, missingName, absolutePath) ==
             RLV_ERR_PATH_ESCAPE,
         "reject traversal and absolute paths outside the package root");
  if (fixture.symlinkReady) {
    expect(rlv_asset_texture_load(store, missingName, symlinkPath) ==
               RLV_ERR_PATH_ESCAPE,
           "reject a symlink escape from the package root");
  }

  const int64_t fontHandle = rlv_asset_font_borrow(font);
  const int64_t soundHandle = rlv_asset_sound_borrow(sound);
  const int64_t musicHandle = rlv_asset_music_borrow(music);
  const int64_t shaderHandle = rlv_asset_shader_borrow(shader);
  expect(fontHandle > 0 && soundHandle > 0 && musicHandle > 0 &&
             shaderHandle > 0,
         "borrow each cached resource through its exact type");
  expect(rlv_texture_unload(heroTexture) == RLV_ERR_RESOURCE_LIVE &&
             rlv_font_unload(fontHandle) == RLV_ERR_RESOURCE_LIVE &&
             rlv_sound_unload(soundHandle) == RLV_ERR_RESOURCE_LIVE &&
             rlv_shader_unload(shaderHandle) == RLV_ERR_RESOURCE_LIVE,
         "prevent callers from unloading store-owned resources");

  const int64_t text = textBuffer("dependent layout");
  const int64_t layout = rlv_font_measure(
      fontHandle, text, 18.0, 0.0, 1.0, 0.0, 0.0, 0,
      RLV_TEXT_OVERFLOW_CLIP);
  expect(layout > 0, "create a live dependent font layout");
  expect(rlv_asset_store_cleanup(store) == RLV_ERR_RESOURCE_LIVE &&
             rlv_asset_store_asset_count(store) == 6,
         "preflight cleanup before changing a store with live dependents");
  expect(rlv_text_layout_destroy(layout) == RLV_OK &&
             rlv_buffer_destroy(text) == RLV_OK,
         "release the dependent layout");

  const int64_t frame = rlv_begin_drawing(window);
  expect(frame > 0 &&
             rlv_asset_store_cleanup(store) == RLV_ERR_INVALID_STATE,
         "reject cleanup during an active frame");
  expect(rlv_abort_drawing(frame) == RLV_OK, "abort the active frame");

  expect(rlv_asset_store_cleanup(store) == RLV_OK,
         "clean the store in dependency-safe order");
  expect(rlv_asset_store_cleanup(store) == RLV_OK,
         "make centralized cleanup idempotent");
  expect(rlv_asset_store_live_count() == 0 &&
             rlv_texture_live_count() == 0 && rlv_font_live_count() == 0 &&
             rlv_sound_live_count() == 0 && rlv_music_live_count() == 0 &&
             rlv_shader_live_count() == 0,
         "cleanup releases every cached physical resource");
  expect(rlv_asset_texture_borrow(hero) == RLV_ERR_STALE_HANDLE &&
             rlv_asset_font_borrow(font) == RLV_ERR_STALE_HANDLE &&
             rlv_asset_sound_borrow(sound) == RLV_ERR_STALE_HANDLE &&
             rlv_asset_music_borrow(music) == RLV_ERR_STALE_HANDLE &&
             rlv_asset_shader_borrow(shader) == RLV_ERR_STALE_HANDLE &&
             rlv_asset_texture_lookup(store, heroName) ==
                 RLV_ERR_STALE_HANDLE,
         "invalidate references and lookup after cleanup");
  expect(rlv_audio_close(audio) == RLV_OK &&
             rlv_window_close(window) == RLV_OK,
         "release store dependencies after centralized cleanup");

  for (int64_t id : {heroName, aliasName, fontName, soundName, musicName,
                     shaderName, missingName, emptyName, texturePath, fontPath,
                     soundPath, musicPath, shaderPath, emptyPath, missingPath,
                     escapePath, absolutePath, symlinkPath}) {
    expect(rlv_buffer_destroy(id) == RLV_OK, "destroy test buffer");
  }
}

void graphicsOnlyAssetStoreCycle() {
  Fixture fixture;
  const int64_t title = textBuffer("WP28 graphics-only assets");
  const int64_t window = rlv_window_open(320, 180, title);
  const int64_t root = textBuffer(fixture.root.generic_string());
  const int64_t store = rlv_asset_store_create(window, 0, root);
  const int64_t textureName = textBuffer("hero");
  const int64_t soundName = textBuffer("click");
  const int64_t musicName = textBuffer("theme");
  const int64_t texturePath = textBuffer("assets/sprite.png");
  const int64_t soundPath = textBuffer("assets/sound.wav");
  const int64_t musicPath = textBuffer("assets/music.ogg");

  expect(window > 0 && store > 0,
         "open a graphics-only store without an audio device");
  const int64_t texture =
      rlv_asset_texture_load(store, textureName, texturePath);
  expect(texture > 0 && rlv_asset_texture_borrow(texture) > 0,
         "load graphics resources without audio");
  expect(rlv_asset_sound_load(store, soundName, soundPath) ==
             RLV_ERR_UNAVAILABLE &&
             rlv_asset_music_load(store, musicName, musicPath) ==
                 RLV_ERR_UNAVAILABLE,
         "report unavailable audio loads from a graphics-only store");
  expect(rlv_window_close(window) == RLV_ERR_RESOURCE_LIVE,
         "graphics-only store still owns its window dependency");
  expect(rlv_asset_store_cleanup(store) == RLV_OK &&
             rlv_asset_store_cleanup(store) == RLV_OK,
         "clean a graphics-only store idempotently");
  expect(rlv_asset_texture_borrow(texture) == RLV_ERR_STALE_HANDLE,
         "invalidate graphics-only references after cleanup");
  expect(rlv_window_close(window) == RLV_OK,
         "close the window after graphics-only cleanup");

  for (int64_t id : {title, root, textureName, soundName, musicName,
                     texturePath, soundPath, musicPath}) {
    expect(rlv_buffer_destroy(id) == RLV_OK,
           "destroy graphics-only test buffer");
  }
}

}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  deterministicAssetStoreCycle();
  graphicsOnlyAssetStoreCycle();
  expect(rlv_buffer_live_count() == 0, "leave no path/name buffers live");
  if (failures == 0) {
    std::cout << "phase28 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase28 adapter test failure(s)\n";
  return 1;
}
