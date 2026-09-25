"""Regenerate the WP7 public API inventory from source declarations."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "ROCKET_3_5_API_INVENTORY.md"
DECL = re.compile(r"^pub\s+(fn|struct|enum|type)\s+([A-Za-z_][A-Za-z0-9_]*)", re.M)

MODULES = [
    "rocket.graphics",
    "rocket.graphics.canvas",
    "rocket.graphics.input",
    "rocket.graphics.shapes",
    "rocket.raylib.safe",
    "rocket.assets",
    "rocket.ui",
    "rocket.ui.controls",
    "rocket.ui.containers",
    "rocket.ui.layout",
    "rocket.ui.render",
    "rocket.ui.styles",
    "rocket.ui.theme",
    "rocket.motion",
]

# Explicit redirects take precedence over a coincidental same-name declaration.
REDIRECT = {
    "Color": "rocket.raylib.safe.Color",
    "Point": "rocket.graphics.Vec2",
    "Rect": "rocket.graphics.Rect",
    "point": "rocket.graphics.vec2",
    "rect": "rocket.graphics.rect",
    "key_space": "rocket.raylib.safe.key_space",
    "key_escape": "rocket.raylib.safe.key_escape",
    "key_right": "rocket.raylib.safe.key_right",
    "key_left": "rocket.raylib.safe.key_left",
    "key_down": "rocket.raylib.safe.key_down_arrow",
    "key_up": "rocket.raylib.safe.key_up_arrow",
    "texture_filter_point": "rocket.raylib.safe.point_filter",
    "texture_filter_bilinear": "rocket.raylib.safe.bilinear_filter",
    "texture_filter_trilinear": "rocket.raylib.safe.trilinear_filter",
    "texture_filter_anisotropic_4x": "rocket.raylib.safe.anisotropic_4x_filter",
    "texture_filter_anisotropic_8x": "rocket.raylib.safe.anisotropic_8x_filter",
    "texture_filter_anisotropic_16x": "rocket.raylib.safe.anisotropic_16x_filter",
    "blend_alpha": "rocket.raylib.safe.alpha_blend",
    "blend_additive": "rocket.raylib.safe.additive_blend",
    "blend_multiplied": "rocket.raylib.safe.multiplied_blend",
    "blend_add_colors": "rocket.raylib.safe.add_colors_blend",
    "blend_subtract_colors": "rocket.raylib.safe.subtract_colors_blend",
    "blend_alpha_premultiplied": "rocket.raylib.safe.premultiplied_alpha_blend",
    "shader_uniform_float": "rocket.raylib.safe.float_uniform",
    "shader_uniform_int": "rocket.raylib.safe.int_uniform",
    "shader_uniform_vec2": "rocket.raylib.safe.vec2_uniform",
    "shader_uniform_color": "rocket.raylib.safe.color_uniform",
    "window_logical_width": "rocket.raylib.safe.window_width",
    "window_logical_height": "rocket.raylib.safe.window_height",
    "window_framebuffer_width": "rocket.raylib.safe.framebuffer_width",
    "window_framebuffer_height": "rocket.raylib.safe.framebuffer_height",
    "window_dpi_scale_x": "rocket.raylib.safe.dpi_scale_x",
    "window_dpi_scale_y": "rocket.raylib.safe.dpi_scale_y",
    "window_display_revision": "rocket.raylib.safe.display_revision",
    "window_screenshot_supported": "rocket.raylib.safe.screenshot_supported",
    "screenshot": "rocket.raylib.safe.save_screenshot",
    "clear": "rocket.raylib.safe.clear_background",
    "draw_rectangle": "rocket.graphics.shapes.draw_rect",
    "draw_rectangle_outline": "rocket.graphics.shapes.draw_rect_outline",
    "draw_rounded_rectangle": "rocket.graphics.shapes.draw_rounded_rect",
    "draw_rounded_rectangle_outline": "rocket.graphics.shapes.draw_rounded_rect_outline",
    "draw_rectangle_gradient_vertical": "rocket.graphics.shapes.draw_gradient_rect",
    "draw_rectangle_gradient_horizontal": "rocket.graphics.shapes.draw_gradient_rect",
    "draw_circle_sector": "rocket.graphics.shapes.draw_sector",
    "draw_circle_gradient": "rocket.graphics.shapes.draw_gradient_circle",
    "draw_bezier_line": "rocket.graphics.shapes.draw_bezier",
    "draw_text": "rocket.raylib.safe.draw_text_layout",
    "draw_font_text": "rocket.raylib.safe.draw_text_layout",
    "is_key_pressed": "rocket.raylib.safe.key_pressed",
    "is_key_down": "rocket.raylib.safe.key_down",
    "is_mouse_pressed": "rocket.graphics.input.pointer_pressed",
    "is_mouse_down": "rocket.graphics.input.pointer_down",
    "is_mouse_released": "rocket.graphics.input.pointer_released",
    "mouse_x": "rocket.graphics.input.pointer_position",
    "mouse_y": "rocket.graphics.input.pointer_position",
    "mouse_framebuffer_x": "rocket.raylib.safe.pointer_framebuffer_x",
    "mouse_framebuffer_y": "rocket.raylib.safe.pointer_framebuffer_y",
    "load_shader_from_memory": "rocket.raylib.safe.load_shader_from_source",
}

UNSUPPORTED = {
    "mouse_left": "Legacy device-code constant; rocket.graphics.input defaults to the primary pointer button.",
    "window_monitor_count": "Monitor enumeration is not part of the supported single-window game contract.",
    "window_current_monitor": "Monitor enumeration is not part of the supported single-window game contract.",
    "monitor_x": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_y": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_width": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_height": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_physical_width": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_physical_height": "Raw monitor geometry is not part of the supported single-window game contract.",
    "monitor_refresh_rate": "Raw monitor details are not part of the supported single-window game contract.",
    "window_monitor_selection_supported": "Explicit monitor selection is not supported by the canonical window API.",
    "set_window_monitor": "Explicit monitor selection is not supported by the canonical window API.",
    "draw_rectangle_gradient_four": "Four-corner gradients are not a canonical primitive; use textures or a shader.",
    "draw_ellipse_outline": "Specialized outline primitive was not promoted; draw an ellipse or use a texture.",
    "draw_ring_outline": "Specialized outline primitive was not promoted; use the canonical ring primitive.",
    "draw_circle_sector_outline": "Specialized outline primitive was not promoted; use the canonical sector primitive.",
    "draw_bezier_quadratic": "Curve variant was not promoted; the canonical curve primitive is draw_bezier.",
    "draw_bezier_cubic": "Curve variant was not promoted; the canonical curve primitive is draw_bezier.",
}

INTERNAL = {
    "version_major": "Adapter/version diagnostic; not needed for normal game feature selection.",
    "version_minor": "Adapter/version diagnostic; not needed for normal game feature selection.",
    "apply_pulse": "Legacy callback test seam; no game runtime use.",
}

OBSOLETE = {
    "black": "Example palette helper; construct a game color explicitly.",
    "white": "Example palette helper; construct a game color explicitly.",
    "sky": "Example palette helper; construct a game color explicitly.",
    "accent": "Example palette helper; construct a game color explicitly.",
    "window_ready": "Successful open_window already establishes a valid Window; later operations return Result errors.",
    "window_resizable": "Startup flag is selected by open_window_quality; current dimensions are queried from the window.",
    "window_high_dpi": "Startup flag is selected by open_window_quality; query live DPI scale instead.",
    "window_msaa4x": "Startup flag is selected by open_window_quality; no separate live state is promised.",
    "window_resize_supported": "set_window_size returns a controlled failure if unavailable.",
    "window_fullscreen_supported": "set_window_fullscreen returns a controlled failure if unavailable.",
    "window_borderless_supported": "set_window_borderless returns a controlled failure if unavailable.",
    "resolve_texture_filter_fallback": "set_texture_filter_with_fallback performs the supported choice and applies it atomically.",
}


def declarations(path: Path) -> list[tuple[str, str]]:
    return DECL.findall(path.read_text(encoding="utf-8"))


def module_path(module: str) -> Path:
    return ROOT / "stdlib" / (module.replace(".", "/") + ".rocket")


def main() -> None:
    catalog = {module: declarations(module_path(module)) for module in MODULES}
    symbol_to_modules: dict[str, list[str]] = {}
    for module, items in catalog.items():
        for _, symbol in items:
            symbol_to_modules.setdefault(symbol, []).append(module)

    old_path = ROOT / "examples/raylib_showcase/src/rocket_raylib.rocket"
    old = declarations(old_path)
    rows = []
    counts = dict.fromkeys(("canonical", "intentionally unsupported", "internal/test-only", "obsolete duplicate"), 0)
    for kind, symbol in old:
        if symbol in REDIRECT:
            category, disposition = "canonical", REDIRECT[symbol]
        elif symbol in UNSUPPORTED:
            category, disposition = "intentionally unsupported", UNSUPPORTED[symbol]
        elif symbol in INTERNAL:
            category, disposition = "internal/test-only", INTERNAL[symbol]
        elif symbol in OBSOLETE:
            category, disposition = "obsolete duplicate", OBSOLETE[symbol]
        elif symbol in symbol_to_modules:
            category = "canonical"
            disposition = symbol_to_modules[symbol][0] + "." + symbol
        else:
            raise SystemExit(f"unclassified showcase symbol: {kind} {symbol}")
        counts[category] += 1
        rows.append(f"| `{kind} {symbol}` | {category} | {disposition} |")

    lines = [
        "# Rocket 3.5 public API inventory",
        "",
        "Generated by `scripts/rocket35_api_inventory.py` from source declarations. "
        "The canonical column is a migration destination, not a claim that the old and new signatures match. "
        "Use the checked `Result` contracts and typed values in the canonical modules.",
        "",
        "## Supported modules",
        "",
        "| Module | Public functions | Public types | Role |",
        "| --- | ---: | ---: | --- |",
    ]
    for module, items in catalog.items():
        fns = sum(kind == "fn" for kind, _ in items)
        types = len(items) - fns
        role = "Game-facing"
        if module == "rocket.raylib.safe":
            role = "Checked native lifetime and drawing"
        lines.append(f"| `{module}` | {fns} | {types} | {role} |")
    lines += [
        "",
        "`rocket.raylib.native` is an internal host binding, not a game-facing module. "
        "The historical `src.rocket_assets` shim delegates to `rocket.assets`; its typed store and reference declarations "
        "have matching canonical declarations. `src.rocket_raylib_testing` is a test seam.",
        "",
        "## Canonical declaration index",
        "",
    ]
    for module, items in catalog.items():
        lines.extend((f"### `{module}`", ""))
        for label, declarations_in_group in (
            ("Types", [f"`{kind} {symbol}`" for kind, symbol in items if kind != "fn"]),
            ("Functions", [f"`fn {symbol}`" for kind, symbol in items if kind == "fn"]),
        ):
            lines.extend((f"**{label}**", ""))
            for index in range(0, len(declarations_in_group), 8):
                lines.append("- " + ", ".join(declarations_in_group[index : index + 8]))
            lines.append("")
    lines += [
        "## Historical showcase wrapper disposition",
        "",
        f"The wrapper has {len(old)} public declarations: "
        + ", ".join(f"{value} {key}" for key, value in counts.items())
        + ".",
        "",
        "| Old declaration | Classification | Supported replacement or reason |",
        "| --- | --- | --- |",
        *rows,
        "",
        "## Other showcase-local public declarations",
        "",
        "| Source | Declaration | Classification | Disposition |",
        "| --- | --- | --- | --- |",
    ]
    assets_shim = declarations(ROOT / "examples/raylib_showcase/src/rocket_assets.rocket")
    canonical_assets = {symbol for _, symbol in catalog["rocket.assets"]}
    for kind, symbol in assets_shim:
        if symbol not in canonical_assets:
            raise SystemExit(f"unclassified asset shim symbol: {kind} {symbol}")
        lines.append(
            f"| `src.rocket_assets` | `{kind} {symbol}` | canonical | "
            f"`rocket.assets.{symbol}`; shim values only translate handle types. |"
        )
    for file, explanation in (
        ("rocket_raylib_testing.rocket", "Native adapter test seam; do not import in games."),
        ("rocket_raylib_callbacks.rocket", "Legacy callback fixture for `apply_pulse`."),
    ):
        for kind, symbol in declarations(ROOT / "examples/raylib_showcase/src" / file):
            lines.append(
                f"| `src.{file[:-7]}` | `{kind} {symbol}` | internal/test-only | {explanation} |"
            )
    lines += [
        "",
        "## Removal gate",
        "",
        "The old `src.rocket_raylib` and `src.rocket_assets` files remain compatibility code while the showcase's "
        "11 tests, legacy package checks, tooling, and historical documentation still import or name them. "
        "Remove them only after those supported consumers have been migrated and rerun. "
        "New games import the supported modules above.",
        "",
    ]
    OUT.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {OUT.relative_to(ROOT)}: {len(old)} classified declarations")


if __name__ == "__main__":
    main()
