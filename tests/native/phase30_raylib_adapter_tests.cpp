#include "rocket_raylib_adapter.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string_view>

namespace {

int failures = 0;

void expect(bool condition, std::string_view message) {
  if (condition) return;
  ++failures;
  std::cerr << "phase30 adapter failure: " << message << '\n';
}

int64_t textBuffer(std::string_view value) {
  const int64_t id = rlv_buffer_create();
  for (unsigned char byte : value) {
    expect(rlv_buffer_push(id, byte) == RLV_OK, "push text byte");
  }
  return id;
}

void measureAndRelease(int64_t font, int64_t text, double size) {
  const int64_t layout = rlv_font_measure(
      font, text, size, 0.0, 1.0, 0.0, 0.0, 0,
      RLV_TEXT_OVERFLOW_CLIP);
  expect(layout > 0, "measure text");
  if (layout > 0) {
    expect(rlv_text_layout_destroy(layout) == RLV_OK,
           "release measured layout");
  }
}

void boundedCachesCycle() {
  const std::filesystem::path root =
      std::filesystem::current_path() / "out" / "rocket-wp30-assets";
  std::error_code error;
  std::filesystem::remove_all(root, error);
  std::filesystem::create_directories(root / "assets", error);
  std::ofstream(root / "assets" / "font.ttf") << "font";
  std::ofstream(root / "assets" / "sprite.png") << "texture";

  const int64_t title = textBuffer("WP30 bounded caches");
  const int64_t window = rlv_window_open(320, 180, title);
  const int64_t audio = rlv_audio_open();
  const int64_t rootBuffer = textBuffer(root.generic_string());
  expect(window > 0 && audio > 0, "open deterministic dependencies");
  const int64_t legacyStore =
      rlv_asset_store_create(window, audio, rootBuffer);
  expect(legacyStore > 0 &&
             rlv_asset_store_capacity(legacyStore) == 256 &&
             rlv_asset_store_cleanup(legacyStore) == RLV_OK &&
             rlv_asset_store_cleanup(legacyStore) == RLV_OK,
         "preserve the Wave B default-capacity asset-store ABI");
  expect(rlv_asset_store_create_bounded(window, audio, rootBuffer, 0) ==
             RLV_ERR_INVALID_ARGUMENT &&
             rlv_asset_store_create_bounded(
                 window, audio, rootBuffer, 100001) ==
                 RLV_ERR_INVALID_ARGUMENT,
         "reject unbounded or excessive resource capacities");
  const int64_t store =
      rlv_asset_store_create_bounded(window, audio, rootBuffer, 2);
  expect(store > 0 && rlv_asset_store_capacity(store) == 2,
         "freeze and expose the explicit resource bound");

  const int64_t fontName = textBuffer("body");
  const int64_t textureName = textBuffer("hero");
  const int64_t overflowName = textBuffer("overflow");
  const int64_t fontPath = textBuffer("assets/font.ttf");
  const int64_t texturePath = textBuffer("assets/sprite.png");
  const int64_t fontRef =
      rlv_asset_font_load(store, fontName, fontPath);
  const int64_t textureRef =
      rlv_asset_texture_load(store, textureName, texturePath);
  expect(fontRef > 0 && textureRef > 0 &&
             rlv_asset_store_asset_count(store) == 2 &&
             rlv_asset_store_physical_count(store) == 2,
         "fill logical and physical resource caches to their bound");
  expect(rlv_asset_texture_load(store, overflowName, texturePath) ==
             RLV_ERR_CAPACITY &&
             rlv_asset_texture_load(store, textureName, texturePath) ==
                 RLV_ERR_DUPLICATE_ASSET &&
             rlv_asset_store_asset_count(store) == 2 &&
             rlv_asset_store_physical_count(store) == 2,
         "report deterministic exhaustion without allocating or hiding duplicates");

  const int64_t text = textBuffer("bounded measurement");
  const int64_t unrelatedFont = rlv_font_default(window);
  const int64_t storeFont = rlv_asset_font_borrow(fontRef);
  measureAndRelease(unrelatedFont, text, 17.0);
  measureAndRelease(storeFont, text, 18.0);
  expect(rlv_font_measurement_cache_size() == 2,
         "cache measurements for unrelated font resources");
  expect(rlv_asset_store_cleanup(store) == RLV_OK &&
             rlv_font_measurement_cache_size() == 1,
         "resource cleanup invalidates only its affected font measurements");
  const int64_t selectiveHits = rlv_font_measurement_cache_hits();
  measureAndRelease(unrelatedFont, text, 17.0);
  expect(rlv_font_measurement_cache_hits() == selectiveHits + 1,
         "unrelated measurements survive selective resource invalidation");

  expect(rlv_font_invalidate_measurements(unrelatedFont) == RLV_OK,
         "clear the calibration font cache");
  expect(rlv_font_measurement_cache_capacity() == 256,
         "freeze the calibrated measurement-cache capacity");
  for (int index = 0; index < 256; ++index) {
    measureAndRelease(unrelatedFont, text, 8.0 + index);
  }
  expect(rlv_font_measurement_cache_size() == 256,
         "fill the measurement cache exactly to capacity");
  measureAndRelease(unrelatedFont, text, 8.0);
  measureAndRelease(unrelatedFont, text, 264.0);
  const int64_t hitsBeforeOldest = rlv_font_measurement_cache_hits();
  const int64_t missesBeforeOldest = rlv_font_measurement_cache_misses();
  measureAndRelease(unrelatedFont, text, 8.0);
  measureAndRelease(unrelatedFont, text, 9.0);
  expect(rlv_font_measurement_cache_hits() == hitsBeforeOldest + 1 &&
             rlv_font_measurement_cache_misses() == missesBeforeOldest + 1 &&
             rlv_font_measurement_cache_size() == 256,
         "evict the least-recently-used measurement deterministically");

  expect(rlv_font_unload(unrelatedFont) == RLV_OK &&
             rlv_font_measurement_cache_size() == 0,
         "font unload removes its remaining cached measurements");
  for (int index = 0; index < 1024; ++index) {
    const int64_t churnStore =
        rlv_asset_store_create_bounded(window, audio, rootBuffer, 1);
    expect(churnStore > 0 && rlv_asset_store_cleanup(churnStore) == RLV_OK &&
               rlv_asset_store_cleanup(churnStore) == RLV_OK,
           "keep repeated asset-store cleanup idempotent");
  }
  expect(rlv_asset_store_live_count() == 0 &&
             rlv_test_asset_store_retired_count() == 0 &&
             rlv_asset_store_cleanup(legacyStore) == RLV_OK,
         "bound retired asset-store tracking without losing idempotence");
  expect(rlv_audio_close(audio) == RLV_OK &&
             rlv_window_close(window) == RLV_OK,
         "close dependencies after bounded cache cleanup");
  for (int64_t id : {title, rootBuffer, fontName, textureName, overflowName,
                     fontPath, texturePath, text}) {
    expect(rlv_buffer_destroy(id) == RLV_OK, "release test buffer");
  }
  std::filesystem::remove_all(root, error);
}

}  // namespace

int main() {
  expect(rlv_enable_test_mode(1) == RLV_OK, "enable test mode");
  expect(rlv_test_reset() == RLV_OK, "reset deterministic backend");
  boundedCachesCycle();
  expect(rlv_buffer_live_count() == 0, "leave no buffers live");
  if (failures == 0) {
    std::cout << "phase30 adapter tests passed successfully\n";
    return 0;
  }
  std::cerr << failures << " phase30 adapter test failure(s)\n";
  return 1;
}
