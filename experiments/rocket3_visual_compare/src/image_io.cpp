#include "image_io.h"

#include <raylib.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>

namespace rocket3::visual_compare {
namespace {

constexpr std::size_t kChannels = 4;

bool valid_dimensions(int width, int height, std::size_t& length) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384) {
        return false;
    }
    const auto w = static_cast<std::size_t>(width);
    const auto h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / h ||
        w * h > std::numeric_limits<std::size_t>::max() / kChannels) {
        return false;
    }
    length = w * h * kChannels;
    return true;
}

bool png_extension(const std::filesystem::path& path) {
    std::string extension = path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
                   [](unsigned char value) {
                       return static_cast<char>(std::tolower(value));
                   });
    return extension == ".png";
}

ImageResult image_failure(std::string message) {
    ImageResult result;
    result.error = std::move(message);
    return result;
}

bool next_ppm_token(std::istream& input, std::string& token) {
    while (input >> token) {
        if (!token.empty() && token[0] == '#') {
            std::string ignored;
            std::getline(input, ignored);
            continue;
        }
        return true;
    }
    return false;
}

bool parse_integer(const std::string& token, int& value) {
    try {
        std::size_t consumed = 0;
        const long parsed = std::stol(token, &consumed, 10);
        if (consumed != token.size() ||
            parsed < std::numeric_limits<int>::min() ||
            parsed > std::numeric_limits<int>::max()) {
            return false;
        }
        value = static_cast<int>(parsed);
        return true;
    } catch (...) {
        return false;
    }
}

ImageRgba artifact_image(const std::vector<std::uint8_t>& pixels,
                         int width, int height) {
    ImageRgba image{width, height, pixels};
    for (std::size_t offset = 3; offset < image.pixels.size();
         offset += kChannels) {
        image.pixels[offset] = 255;
    }
    return image;
}

} // namespace

bool ImageRgba::valid() const {
    std::size_t expected = 0;
    return valid_dimensions(width, height, expected) &&
           pixels.size() == expected;
}

ImageResult read_png(const std::filesystem::path& path) {
    if (!png_extension(path)) return image_failure("image path must end in .png");
    if (!std::filesystem::is_regular_file(path)) {
        return image_failure("PNG image does not exist");
    }
    Image native = LoadImage(path.string().c_str());
    if (!IsImageValid(native)) return image_failure("could not decode PNG image");
    if (native.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8) {
        ImageFormat(&native, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    }
    std::size_t length = 0;
    if (!IsImageValid(native) ||
        !valid_dimensions(native.width, native.height, length)) {
        UnloadImage(native);
        return image_failure("decoded PNG dimensions are invalid");
    }
    const auto* bytes = static_cast<const std::uint8_t*>(native.data);
    ImageResult result;
    result.ok = true;
    result.image.width = native.width;
    result.image.height = native.height;
    result.image.pixels.assign(bytes, bytes + length);
    UnloadImage(native);
    return result;
}

bool write_png(const std::filesystem::path& path,
               const ImageRgba& image,
               std::string& error) {
    if (!image.valid()) {
        error = "RGBA image is invalid";
        return false;
    }
    if (!png_extension(path)) {
        error = "image path must end in .png";
        return false;
    }
    if (!path.parent_path().empty() &&
        !std::filesystem::is_directory(path.parent_path())) {
        error = "image output directory does not exist";
        return false;
    }
    Image native{};
    native.data = const_cast<std::uint8_t*>(image.pixels.data());
    native.width = image.width;
    native.height = image.height;
    native.mipmaps = 1;
    native.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    if (!ExportImage(native, path.string().c_str())) {
        error = "could not encode PNG image";
        return false;
    }
    return true;
}

ImageResult read_ppm_reference(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) return image_failure("golden reference does not exist");
    std::string token;
    if (!next_ppm_token(input, token) || token != "P3") {
        return image_failure("golden reference must be P3 PPM");
    }
    int width = 0;
    int height = 0;
    int maximum = 0;
    if (!next_ppm_token(input, token) || !parse_integer(token, width) ||
        !next_ppm_token(input, token) || !parse_integer(token, height) ||
        !next_ppm_token(input, token) || !parse_integer(token, maximum) ||
        maximum != 255) {
        return image_failure("golden reference header is invalid");
    }
    std::size_t length = 0;
    if (!valid_dimensions(width, height, length)) {
        return image_failure("golden reference dimensions are invalid");
    }
    ImageResult result;
    result.image = ImageRgba{width, height, {}};
    result.image.pixels.reserve(length);
    for (std::size_t pixel = 0; pixel < length / kChannels; ++pixel) {
        for (int channel = 0; channel < 3; ++channel) {
            int value = 0;
            if (!next_ppm_token(input, token) || !parse_integer(token, value) ||
                value < 0 || value > 255) {
                return image_failure("golden reference pixel data is invalid");
            }
            result.image.pixels.push_back(static_cast<std::uint8_t>(value));
        }
        result.image.pixels.push_back(255);
    }
    if (next_ppm_token(input, token)) {
        return image_failure("golden reference contains trailing pixel data");
    }
    result.ok = true;
    return result;
}

bool write_ppm_reference(const std::filesystem::path& path,
                         const ImageRgba& image,
                         std::string& error) {
    if (!image.valid()) {
        error = "RGBA image is invalid";
        return false;
    }
    if (path.extension() != ".ppm") {
        error = "golden reference path must end in .ppm";
        return false;
    }
    if (!path.parent_path().empty() &&
        !std::filesystem::is_directory(path.parent_path())) {
        error = "golden output directory does not exist";
        return false;
    }
    std::ofstream output(path, std::ios::trunc);
    if (!output) {
        error = "could not open golden reference for writing";
        return false;
    }
    output << "P3\n# Rocket 3 WP32 reviewed canonical reference\n"
           << image.width << ' ' << image.height << "\n255\n";
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * image.width + x) * kChannels;
            output << static_cast<int>(image.pixels[offset]) << ' '
                   << static_cast<int>(image.pixels[offset + 1]) << ' '
                   << static_cast<int>(image.pixels[offset + 2]);
            output << (x + 1 == image.width ? '\n' : ' ');
        }
    }
    if (!output) {
        error = "could not write golden reference";
        return false;
    }
    return true;
}

bool write_comparison_artifacts(const std::filesystem::path& directory,
                                const ImageRgba& actual,
                                const Comparison& comparison,
                                std::string& error) {
    if (!actual.valid() || !comparison.ok ||
        comparison.metrics.difference.size() != actual.pixels.size() ||
        comparison.metrics.heat.size() != actual.pixels.size()) {
        error = "comparison artifact inputs are invalid";
        return false;
    }
    std::error_code filesystem_error;
    std::filesystem::create_directories(directory, filesystem_error);
    if (filesystem_error) {
        error = "could not create comparison artifact directory";
        return false;
    }
    const ImageRgba difference = artifact_image(
        comparison.metrics.difference, actual.width, actual.height);
    const ImageRgba heat = artifact_image(
        comparison.metrics.heat, actual.width, actual.height);
    if (!write_png(directory / "generated.png", actual, error) ||
        !write_png(directory / "difference.png", difference, error) ||
        !write_png(directory / "heatmap.png", heat, error)) {
        return false;
    }
    std::ofstream metrics(directory / "metrics.json", std::ios::trunc);
    if (!metrics) {
        error = "could not open metrics artifact";
        return false;
    }
    metrics << std::fixed << std::setprecision(6)
            << "{\n  \"changed_pixels\": "
            << comparison.metrics.changed_pixels
            << ",\n  \"changed_pixel_ratio\": "
            << comparison.metrics.changed_pixel_ratio
            << ",\n  \"mean_absolute_error\": "
            << comparison.metrics.mean_absolute_error
            << ",\n  \"mean_absolute_error_by_channel\": [";
    for (std::size_t channel = 0; channel < kChannels; ++channel) {
        if (channel != 0) metrics << ", ";
        metrics << comparison.metrics.mean_absolute_error_by_channel[channel];
    }
    metrics << "],\n  \"max_channel_delta\": "
            << static_cast<int>(comparison.metrics.max_channel_delta)
            << ",\n  \"max_delta_by_channel\": [";
    for (std::size_t channel = 0; channel < kChannels; ++channel) {
        if (channel != 0) metrics << ", ";
        metrics << static_cast<int>(
            comparison.metrics.max_delta_by_channel[channel]);
    }
    const Bounds& bounds = comparison.metrics.changed_bounds;
    metrics << "],\n  \"changed_bounds\": {\"has_pixels\": "
            << (bounds.has_pixels ? "true" : "false")
            << ", \"min_x\": " << bounds.min_x
            << ", \"min_y\": " << bounds.min_y
            << ", \"max_x\": " << bounds.max_x
            << ", \"max_y\": " << bounds.max_y << "}\n}\n";
    if (!metrics) {
        error = "could not write metrics artifact";
        return false;
    }
    return true;
}

bool update_approved_golden(const std::filesystem::path& path,
                            const ImageRgba& image,
                            const GoldenApproval& approval,
                            std::string& error) {
    if (approval.status != GoldenApproval::Status::Approved ||
        !approval.update_allowed || approval.approval_id.empty() ||
        approval.scene_key.empty() || approval.baseline_hash.empty() ||
        approval.approved_by.empty() || approval.approved_at.empty()) {
        error = "golden update requires an explicit complete approval record";
        return false;
    }
    return write_ppm_reference(path, image, error);
}

} // namespace rocket3::visual_compare
