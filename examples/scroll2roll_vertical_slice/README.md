# Scroll2Roll vertical slice

This is a Rocket 3.5 acceptance scene, not the casino product. It uses only
bundled game-facing modules, one `rocket.assets` store, and the production
Raylib adapter. It has a 960×540 logical layout, a resizable high-DPI window,
live rendered button states, animated cards and chips, target composition,
additive blending, a shader with an unshaded capability fallback, sound,
looping music, and checked cleanup.

From this package directory, run `rocketc check .` and `rocketc build .` using
the Rocket SDK and its packaged native libraries. Run the resulting executable
from this directory so the default asset root `.` resolves to these assets.
For a launcher or relocated package running elsewhere, set
`ROCKET_SLICE_PACKAGE_ROOT` to the absolute package directory.
Move the pointer over **PLAY PREVIEW** or press it to hear the click. Close the
window to save `scroll2roll-slice.png` (logical target) and
`scroll2roll-presented.png` (physical framebuffer). Set the environment
variable `ROCKET_SLICE_BENCHMARK=1` for an automatic 440-frame run. Its first
200 frames exercise two resizes and a fullscreen round trip; the final 240
frames report average and worst frame time in milliseconds. The benchmark
uses a 1000 FPS target to reduce timing-pacer influence; normal interactive
mode targets 60. Benchmark motion is fixed at 0.6 seconds for repeatable
visual comparisons, while interactive motion uses real elapsed time.

The store owns four textures, one sound, one music stream, one font, and an
optional shader (seven or eight resources). The logical render target is
separately owned. The scene makes 17 top-level drawing submissions per frame,
including two clears and target presentation; panel, button, and text rendering
expand those submissions internally. Each frame has one target pass, one
additive blend scope, and, when supported, one shader presentation scope. The
screenshot is taken after the frame ends; all resources are then released before the
audio device and window close.

`assets/table-background.png` and `assets/card-back.png` were generated for
this acceptance scene using the built-in image generation tool. Prompts:

- Table: top-down, orthographic, wide deep emerald velvet casino table with a
  restrained brass and walnut edge, clear central space, no text or objects.
- Card back: flat portrait playing-card back with symmetric brass filigree on
  forest green and an ivory border, no text or perspective.

The card-front atlas, chips, audio samples, and shader are deterministic
Rocket repository assets reused from `examples/rocket35_audio`. The bundled
`Basic-Regular.ttf` font comes from [Google Fonts](https://github.com/google/fonts/tree/main/ofl/basic)
under the SIL Open Font License included as `assets/BASIC-OFL.txt`.
