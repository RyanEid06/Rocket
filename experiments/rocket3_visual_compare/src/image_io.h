#pragma once

#include "comparator.h"
#include "evidence_schema.h"

#include <filesystem>
#include <string>
#include <vector>

namespace rocket3::visual_compare {

struct ImageRgba {
    int width = 0;
    int height = 0;
    std::vector<std::uint8_t> pixels;

    bool valid() const;
};

struct ImageResult {
    bool ok = false;
    std::string error;
    ImageRgba image;
};

ImageResult read_png(const std::filesystem::path& path);
bool write_png(const std::filesystem::path& path,
               const ImageRgba& image,
               std::string& error);
ImageResult read_ppm_reference(const std::filesystem::path& path);
bool write_ppm_reference(const std::filesystem::path& path,
                         const ImageRgba& image,
                         std::string& error);
bool write_comparison_artifacts(const std::filesystem::path& directory,
                                const ImageRgba& actual,
                                const Comparison& comparison,
                                std::string& error);
bool update_approved_golden(const std::filesystem::path& path,
                            const ImageRgba& image,
                            const GoldenApproval& approval,
                            std::string& error);

} // namespace rocket3::visual_compare
