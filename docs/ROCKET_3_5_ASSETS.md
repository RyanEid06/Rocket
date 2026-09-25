# Rocket 3.5 assets

Import `rocket.assets` alongside `rocket.raylib.safe`. A store owns every
resource it loads. `TextureRef`, `FontRef`, `SoundRef`, `MusicRef`, and
`ShaderRef` are typed borrowed references. `borrow_*` returns the corresponding
canonical `safe` handle for drawing, playback, and shader operations; it does
not transfer ownership. Calling `safe.unload_*` on a store-owned handle returns
`raylib: owned resource is still live`.

## Create and load

Call `assets.open_graphics(window, package_root, capacity: 256)` for a game
without audio. For sounds and music, open a `safe.AudioDevice` first and call
`assets.open(window, audio, package_root, capacity: 256)`. Capacity is the
number of logical names, between 1 and 100,000. Graphics-only stores reject
sound and music loads with `raylib: device or backend unavailable`.

`package_root` is required and must name an existing directory. Pass its
absolute path so launch location cannot change asset resolution. Paths passed to
`load_texture`, `load_font`, `load_sound`, `load_music`, and `load_shader` are
relative to that root. The native store normalizes paths, rejects absolute,
traversal, and symlink escapes, and returns a controlled error for missing or
unloadable files. A failed load leaves the store unchanged.

```rocket
import rocket.assets
import rocket.raylib.safe

fn load_art(window: safe.Window, package_root: String) -> Result[assets.AssetStore, String]:
    match assets.open_graphics(window, package_root):
        case Err(message):
            return Err(message)
        case Ok(store):
            match assets.load_texture(store, "card-back", "assets/card-back.ppm"):
                case Err(message):
                    let cleaned = assets.cleanup(store)
                    return Err(message)
                case Ok(reference):
                    return Ok(store)
```

Look up an existing name with `assets.texture(store, "card-back")`, then call
`assets.borrow_texture(reference)` before drawing. Equivalent lookup and
borrow functions exist for the other four resource types. A second load with
the same logical name fails with `raylib: duplicate asset name`; a lookup using
the wrong type fails with `raylib: asset type mismatch`. Different names for
the same normalized path share one native resource. `assets.capacity(store)`
returns the configured limit.

For shaders, call `assets.load_shader(store, name, vertex_path,
fragment_path)` with at least one nonempty source path. The returned borrow is
the same `safe.Shader` used by `safe.begin_shader` and uniform functions. Sound
and music borrows are the same `safe.Sound` and `safe.Music` used by the WP4
audio API. Streamed music still needs `safe.update_music(music)` every frame
while playing.

## Cleanup order

End frames and scopes, then call `assets.cleanup(store)` before closing its
audio device or window. Cleanup unloads each physical resource once and
invalidates every reference from that store. A later `borrow_*` returns
`raylib: stale or already released handle`. Cleanup is idempotent. A live text
layout holding a store-owned font causes cleanup to return
`raylib: owned resource is still live`; release the layout and retry. Resources
created directly with `safe.load_*` remain separately owned and must be
unloaded through `safe`.

The canonical tests in `tests/fixtures/rocket35_assets_package` run through
the adapter's deterministic device seam. They pass an absolute package root
while running from a different working directory, so the path rule is tested
independently of the launch location.
