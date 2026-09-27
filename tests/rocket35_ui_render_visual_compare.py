"""Compare WP6 PNG captures without third-party image packages."""

import hashlib
import json
import struct
import sys
import zlib
from pathlib import Path


def pixels(path):
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError(f"{path}: invalid PNG")
    pos = 8
    compressed = bytearray()
    width = height = channels = None
    while pos < len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        kind = data[pos + 4 : pos + 8]
        body = data[pos + 8 : pos + 8 + length]
        pos += length + 12
        if kind == b"IHDR":
            width, height, depth, color, compression, filtering, interlace = struct.unpack(
                ">IIBBBBB", body
            )
            if depth != 8 or color not in (2, 6) or interlace != 0:
                raise ValueError(f"{path}: unsupported PNG layout")
            channels = 4 if color == 6 else 3
        elif kind == b"IDAT":
            compressed.extend(body)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(compressed)
    stride = width * channels
    output = bytearray()
    previous = bytearray(stride)
    pos = 0
    for _ in range(height):
        filter_type = raw[pos]
        pos += 1
        row = bytearray(raw[pos : pos + stride])
        pos += stride
        for x in range(stride):
            left = row[x - channels] if x >= channels else 0
            up = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if filter_type == 1:
                predictor = left
            elif filter_type == 2:
                predictor = up
            elif filter_type == 3:
                predictor = (left + up) // 2
            elif filter_type == 4:
                estimate = left + up - upper_left
                distances = (abs(estimate - left), abs(estimate - up), abs(estimate - upper_left))
                predictor = (left, up, upper_left)[distances.index(min(distances))]
            elif filter_type == 0:
                predictor = 0
            else:
                raise ValueError(f"{path}: unsupported PNG filter {filter_type}")
            row[x] = (row[x] + predictor) & 255
        output.extend(row)
        previous = row
    return width, height, channels, output


def main():
    manifest_path, golden_root, capture_root = map(Path, sys.argv[1:4])
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    if manifest["schema"] != "rocket35-wp6-ui-goldens-v1" or len(manifest["scenes"]) != 5:
        raise ValueError("WP6 visual manifest is incomplete")
    for scene in manifest["scenes"]:
        reference = golden_root / scene["reference"]
        capture = capture_root / scene["reference"]
        if hashlib.sha256(reference.read_bytes()).hexdigest() != scene["sha256"]:
            raise ValueError(f"{reference}: golden hash changed without review")
        expected = pixels(reference)
        actual = pixels(capture)
        if expected[:3] != actual[:3]:
            raise ValueError(f"{capture}: dimensions or channels differ")
        differences = [abs(a - b) for a, b in zip(expected[3], actual[3])]
        changed = sum(any(differences[i : i + expected[2]]) for i in range(0, len(differences), expected[2]))
        mean = sum(differences) / len(differences)
        ratio = changed / (expected[0] * expected[1])
        tolerance = scene["tolerance"]
        print(f"{scene['name']}: mean={mean:.3f} changed={ratio:.4f}")
        if mean > tolerance["mean_absolute_error"] or ratio > tolerance["changed_pixel_ratio"]:
            raise ValueError(f"{capture}: visual regression exceeds reviewed tolerance")
        for x, y in scene.get("anchors", []):
            start = (y * expected[0] + x) * expected[2]
            reference_pixel = expected[3][start : start + expected[2]]
            capture_pixel = actual[3][start : start + actual[2]]
            if any(abs(a - b) > tolerance["anchor_channel_delta"] for a, b in zip(reference_pixel, capture_pixel)):
                raise ValueError(f"{capture}: visual anchor changed at {x},{y}")
        for x, y, width, height, minimum in scene.get("lit_regions", []):
            lit = 0
            for row in range(y, y + height):
                for column in range(x, x + width):
                    start = (row * actual[0] + column) * actual[2]
                    if min(actual[3][start : start + 3]) > 150:
                        lit += 1
            if lit < minimum:
                raise ValueError(f"{capture}: visual content missing from {x},{y},{width},{height}")


if __name__ == "__main__":
    main()
