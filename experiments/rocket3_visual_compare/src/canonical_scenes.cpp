#include "canonical_scenes.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>

namespace rocket3::visual_compare {
namespace {

struct Color {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
    std::uint8_t alpha = 255;
};

ImageRgba canvas(int width, int height, Color color) {
    ImageRgba image{width, height, {}};
    image.pixels.resize(static_cast<std::size_t>(width) * height * 4);
    for (std::size_t offset = 0; offset < image.pixels.size(); offset += 4) {
        image.pixels[offset] = color.red;
        image.pixels[offset + 1] = color.green;
        image.pixels[offset + 2] = color.blue;
        image.pixels[offset + 3] = color.alpha;
    }
    return image;
}

void pixel(ImageRgba& image, int x, int y, Color color) {
    if (x < 0 || y < 0 || x >= image.width || y >= image.height) return;
    const std::size_t offset =
        (static_cast<std::size_t>(y) * image.width + x) * 4;
    const int inverse = 255 - color.alpha;
    image.pixels[offset] = static_cast<std::uint8_t>(
        (color.red * color.alpha + image.pixels[offset] * inverse + 127) /
        255);
    image.pixels[offset + 1] = static_cast<std::uint8_t>(
        (color.green * color.alpha + image.pixels[offset + 1] * inverse +
         127) /
        255);
    image.pixels[offset + 2] = static_cast<std::uint8_t>(
        (color.blue * color.alpha + image.pixels[offset + 2] * inverse +
         127) /
        255);
    image.pixels[offset + 3] = 255;
}

void rectangle(ImageRgba& image, int x, int y, int width, int height,
               Color color, int clip_x = 0, int clip_y = 0,
               int clip_width = 100000, int clip_height = 100000) {
    const int left = std::max({0, x, clip_x});
    const int top = std::max({0, y, clip_y});
    const int right = std::min({image.width, x + width, clip_x + clip_width});
    const int bottom =
        std::min({image.height, y + height, clip_y + clip_height});
    for (int py = top; py < bottom; ++py) {
        for (int px = left; px < right; ++px) pixel(image, px, py, color);
    }
}

void rectangle_outline(ImageRgba& image, int x, int y, int width, int height,
                       Color color) {
    rectangle(image, x, y, width, 1, color);
    rectangle(image, x, y + height - 1, width, 1, color);
    rectangle(image, x, y, 1, height, color);
    rectangle(image, x + width - 1, y, 1, height, color);
}

void rounded_rectangle(ImageRgba& image, int x, int y, int width, int height,
                       int radius, Color color) {
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const int nearest_x =
                std::clamp(px, radius, std::max(radius, width - radius - 1));
            const int nearest_y =
                std::clamp(py, radius, std::max(radius, height - radius - 1));
            const int dx = px - nearest_x;
            const int dy = py - nearest_y;
            if (dx * dx + dy * dy <= radius * radius) {
                pixel(image, x + px, y + py, color);
            }
        }
    }
}

void gradient(ImageRgba& image, int x, int y, int width, int height,
              Color top, Color bottom) {
    for (int row = 0; row < height; ++row) {
        const int denominator = std::max(1, height - 1);
        const auto mix = [&](std::uint8_t first, std::uint8_t second) {
            return static_cast<std::uint8_t>(
                (first * (denominator - row) + second * row +
                 denominator / 2) /
                denominator);
        };
        rectangle(image, x, y + row, width, 1,
                  Color{mix(top.red, bottom.red),
                        mix(top.green, bottom.green),
                        mix(top.blue, bottom.blue),
                        mix(top.alpha, bottom.alpha)});
    }
}

void ring_sector(ImageRgba& image, int center_x, int center_y,
                 int inner_radius, int outer_radius, Color color) {
    const int inner_squared = inner_radius * inner_radius;
    const int outer_squared = outer_radius * outer_radius;
    for (int y = -outer_radius; y <= outer_radius; ++y) {
        for (int x = -outer_radius; x <= outer_radius; ++x) {
            const int squared = x * x + y * y;
            const bool in_three_quarter_sector = y <= 0 || x >= 0;
            if (squared >= inner_squared && squared <= outer_squared &&
                in_three_quarter_sector) {
                pixel(image, center_x + x, center_y + y, color);
            }
        }
    }
}

void transformed_checker(ImageRgba& image, int x, int y, int width,
                         int height) {
    const std::array<Color, 4> colors = {
        Color{214, 93, 121}, Color{78, 169, 224},
        Color{240, 199, 94}, Color{91, 207, 151}};
    for (int py = 0; py < height; ++py) {
        for (int px = 0; px < width; ++px) {
            const int source_x = py * 4 / std::max(1, height);
            const int source_y = (width - px - 1) * 4 / std::max(1, width);
            pixel(image, x + px, y + py,
                  colors[static_cast<std::size_t>(
                      (source_x + source_y * 2) & 3)]);
        }
    }
}

void filtered_strip(ImageRgba& image, int x, int y, int width) {
    const Color left{34, 111, 189};
    const Color right{233, 183, 67};
    for (int index = 0; index < width; ++index) {
        const int denominator = std::max(1, width - 1);
        const auto mix = [&](std::uint8_t first, std::uint8_t second) {
            return static_cast<std::uint8_t>(
                (first * (denominator - index) + second * index +
                 denominator / 2) /
                denominator);
        };
        pixel(image, x + index, y,
              Color{mix(left.red, right.red), mix(left.green, right.green),
                    mix(left.blue, right.blue)});
    }
}

void glyph(ImageRgba& image, int x, int y, unsigned char value, Color color) {
    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 2; ++column) {
            const unsigned int bit =
                (static_cast<unsigned int>(value) >> (row * 2 + column)) & 1U;
            if (bit != 0) pixel(image, x + column, y + row, color);
        }
    }
}

void text(ImageRgba& image, int x, int y, int width, std::string_view value,
          int alignment, Color color) {
    const int text_width = static_cast<int>(value.size()) * 3 - 1;
    int cursor = x;
    if (alignment == 1) cursor = x + (width - text_width) / 2;
    if (alignment == 2) cursor = x + width - text_width;
    for (unsigned char character : value) {
        glyph(image, cursor, y, character, color);
        cursor += 3;
    }
}

void shader_tint(ImageRgba& image, int x, int y, int width, int height,
                 Color tint) {
    for (int py = y; py < y + height; ++py) {
        for (int px = x; px < x + width; ++px) {
            if (px < 0 || py < 0 || px >= image.width || py >= image.height) {
                continue;
            }
            const std::size_t offset =
                (static_cast<std::size_t>(py) * image.width + px) * 4;
            image.pixels[offset] = static_cast<std::uint8_t>(
                image.pixels[offset] * tint.red / 255);
            image.pixels[offset + 1] = static_cast<std::uint8_t>(
                image.pixels[offset + 1] * tint.green / 255);
            image.pixels[offset + 2] = static_cast<std::uint8_t>(
                image.pixels[offset + 2] * tint.blue / 255);
        }
    }
}

CanonicalScene geometry_scene() {
    auto image = canvas(16, 12, Color{16, 21, 31});
    gradient(image, 0, 0, 16, 12, Color{18, 31, 51}, Color{41, 24, 56});
    rounded_rectangle(image, 1, 1, 8, 6, 2, Color{52, 105, 171});
    rectangle_outline(image, 1, 1, 8, 6, Color{153, 205, 255});
    ring_sector(image, 12, 4, 1, 3, Color{240, 179, 74});
    rectangle(image, 3, 7, 11, 4, Color{214, 74, 109, 150}, 5, 8, 7, 2);
    rectangle(image, 7, 8, 7, 3, Color{63, 210, 154, 150});
    return {"geometry", std::move(image),
            {"geometry", "rounded-geometry", "outlines", "rings-sectors",
             "gradients", "clipping", "blending"},
            true};
}

CanonicalScene surface_scene() {
    auto image = canvas(16, 12, Color{13, 18, 28});
    rectangle(image, 1, 1, 14, 10, Color{29, 42, 61});
    transformed_checker(image, 1, 1, 6, 5);
    filtered_strip(image, 8, 2, 7);
    filtered_strip(image, 8, 3, 7);
    text(image, 1, 7, 14, "AB", 0, Color{238, 241, 246});
    text(image, 1, 7, 14, "CD", 1, Color{130, 205, 255});
    text(image, 1, 7, 14, "EF", 2, Color{249, 194, 92});
    rectangle_outline(image, 0, 0, 16, 12, Color{70, 90, 118});
    shader_tint(image, 8, 1, 7, 5, Color{190, 230, 255});
    return {"surfaces", std::move(image),
            {"typography-alignments", "transformed-textures", "filtering",
             "shaders", "virtual-canvas"},
            false};
}

CanonicalScene ui_scene() {
    auto image = canvas(16, 12, Color{19, 24, 34});
    rounded_rectangle(image, 1, 1, 14, 10, 2, Color{35, 44, 60});
    rectangle(image, 2, 2, 3, 7, Color{52, 66, 88});
    rectangle(image, 6, 2, 8, 2, Color{70, 91, 119});
    rounded_rectangle(image, 6, 5, 3, 3, 1, Color{54, 137, 221});
    rounded_rectangle(image, 10, 5, 4, 3, 1, Color{79, 93, 114});
    rectangle_outline(image, 10, 5, 4, 3, Color{151, 164, 183});
    rectangle(image, 12, 2, 2, 2, Color{241, 184, 70});
    text(image, 2, 9, 12, "UI", 1, Color{235, 240, 247});
    return {"ui", std::move(image),
            {"layouts", "anchors", "control-states"}, true};
}

CanonicalScene premium_scene() {
    auto image = canvas(24, 16, Color{11, 16, 23});
    gradient(image, 0, 0, 24, 16, Color{14, 27, 38}, Color{22, 41, 43});
    rounded_rectangle(image, 1, 1, 22, 14, 2, Color{24, 48, 49});
    rectangle_outline(image, 2, 2, 20, 12, Color{69, 103, 96});
    rounded_rectangle(image, 3, 3, 5, 7, 1, Color{230, 225, 208});
    rounded_rectangle(image, 9, 3, 5, 7, 1, Color{223, 218, 202});
    rounded_rectangle(image, 15, 3, 5, 7, 1, Color{235, 230, 214});
    text(image, 3, 4, 5, "A", 1, Color{40, 48, 55});
    text(image, 9, 4, 5, "K", 1, Color{151, 49, 62});
    text(image, 15, 4, 5, "Q", 1, Color{40, 48, 55});
    ring_sector(image, 5, 12, 0, 2, Color{217, 76, 89});
    ring_sector(image, 10, 12, 0, 2, Color{65, 136, 222});
    ring_sector(image, 15, 12, 0, 2, Color{232, 181, 62});
    rounded_rectangle(image, 18, 11, 4, 3, 1, Color{70, 160, 124});
    rectangle_outline(image, 18, 11, 4, 3, Color{149, 224, 189});
    return {"premium", std::move(image), {"premium-showcase"}, false};
}

} // namespace

std::vector<CanonicalScene> render_canonical_scenes() {
    std::vector<CanonicalScene> scenes;
    scenes.reserve(4);
    scenes.push_back(geometry_scene());
    scenes.push_back(surface_scene());
    scenes.push_back(ui_scene());
    scenes.push_back(premium_scene());
    return scenes;
}

} // namespace rocket3::visual_compare
