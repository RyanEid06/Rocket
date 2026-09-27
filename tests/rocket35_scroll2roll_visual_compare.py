"""Compare a WP7 logical screenshot with its reviewed, source-owned golden."""

import hashlib
import json
import struct
import sys
from pathlib import Path

from rocket35_ui_render_visual_compare import pixels


def main() -> None:
    manifest_path, golden_root, capture, presented = map(Path, sys.argv[1:5])
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["schema"] != "rocket35-wp7-scroll2roll-v1":
        raise ValueError("unexpected WP7 visual manifest")
    golden = golden_root / manifest["reference"]
    if hashlib.sha256(golden.read_bytes()).hexdigest() != manifest["sha256"]:
        raise ValueError("WP7 golden changed without manifest review")
    expected = pixels(golden)
    actual = pixels(capture)
    if expected[:3] != actual[:3] or list(actual[:2]) != manifest["size"]:
        raise ValueError("WP7 screenshot dimensions or channel layout changed")
    differences = [abs(a - b) for a, b in zip(expected[3], actual[3])]
    channels = expected[2]
    changed = sum(
        any(differences[index : index + channels])
        for index in range(0, len(differences), channels)
    )
    mean = sum(differences) / len(differences)
    ratio = changed / (actual[0] * actual[1])
    print(f"wp7 logical visual: mean={mean:.3f} changed={ratio:.4f}")
    tolerance = manifest["tolerance"]
    if mean > tolerance["mean_absolute_error"] or ratio > tolerance["changed_pixel_ratio"]:
        raise ValueError("WP7 visual regression exceeds reviewed tolerance")
    physical = presented.read_bytes()
    if physical[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("WP7 physical screenshot is not PNG")
    width, height = struct.unpack(">II", physical[16:24])
    if width < 960 or height < 540:
        raise ValueError(f"WP7 physical framebuffer has unexpected size: {width}x{height}")
    print(f"wp7 physical framebuffer: {width}x{height}")


if __name__ == "__main__":
    main()
