# Rocket 3 graphics and UI examples

This package is the focused Rocket 3 graphics/UI gallery. Every source imports
only public `std.*` and `rocket.*` modules.

- `examples/motion_and_color.rocket` demonstrates named/default calls,
  `std.math`, color helpers, easing, motion sampling, and reduced motion.
- `examples/layout_and_controls.rocket` demonstrates VirtualCanvas fitting,
  transforms, responsive rows, anchors, typography measurement, themed
  controls, a logical texture asset, and drawing a resolved button through
  `rocket.ui.render`.
- `examples/premium_card_table.rocket` is the runnable one-frame neutral
  showcase. It
  composes dark surfaces, rounded panels, borders, typography, gradients,
  neutral card-like artwork, buttons, chips, hover state, motion policy, and
  virtual-resolution clipping. It loads the card accent through the
  bundled `rocket.assets` graphics-only store and releases that store before
  closing the window. Its reusable table panel and button use
  `rocket.ui.render`; its cards, chips, and accent remain custom art.

See `docs/ROCKET_3_5_UI_RENDER.md` for the rendered widget API and its logical
canvas, image fit, clipping, and asset ownership rules.

The premium gallery contains no game or wagering rules and is not evidence of
Scroll2Roll visual fidelity.

From the repository root after building the compiler and native adapter:

```powershell
$env:ROCKET_NATIVE_LIBRARY_ROOT = "$PWD/out/build/windows-release/native/windows-x64"
$env:ROCKET_PREMIUM_PACKAGE_ROOT = (Resolve-Path examples/rocket3_graphics_ui).Path
out/build/windows-release/rocketc.exe check examples/rocket3_graphics_ui
out/build/windows-release/rocketc.exe run examples/rocket3_graphics_ui
```

The premium run opens a 960×540 window, renders through a 960×540
VirtualCanvas, writes `rocket3-premium-showcase.png` in the current directory,
and closes after the captured frame.

Use `rocketc check` on either of the other two files for the focused API
walkthroughs. The WP33 acceptance gate also builds and runs all three from a
clean relocated copy of this package.
