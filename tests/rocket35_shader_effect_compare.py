"""Check the WP3 tint shader against the same unshaded scene."""

import sys
from pathlib import Path

from rocket35_ui_render_visual_compare import pixels


def main() -> None:
    off = pixels(Path(sys.argv[1]))
    on = pixels(Path(sys.argv[2]))
    if off[:3] != on[:3]:
        raise ValueError("effect-off and effect-on captures must match in size")

    width, height, channels, before = off
    after = on[3]
    eligible = 0
    matching = 0
    changed = 0
    # The tint shader applies warmth=0.35 only to the presented target. Use
    # interior midtone pixels to avoid clipping and texture edge filtering.
    for y in range(height // 5, height * 4 // 5):
        for x in range(width // 5, width * 4 // 5):
            index = (y * width + x) * channels
            red, green, blue = before[index : index + 3]
            if not (20 <= red <= 170 and 20 <= green <= 170 and 20 <= blue <= 170):
                continue
            eligible += 1
            expected = (min(255, round(red * 1.35)),
                        min(255, round(green * 1.049)),
                        min(255, round(blue * 0.9125)))
            observed = after[index : index + 3]
            if max(abs(a - b) for a, b in zip(expected, observed)) <= 12:
                matching += 1
            if observed[0] >= red + 4 and observed[2] <= blue - 2:
                changed += 1
    print(f"WP3 shader: eligible={eligible} formula_match={matching} warm_direction={changed}")
    if eligible < 1000 or matching < eligible * 0.75 or changed < eligible * 0.5:
        raise ValueError("the effect-on scene does not match the reviewed tint shader")


if __name__ == "__main__":
    main()
