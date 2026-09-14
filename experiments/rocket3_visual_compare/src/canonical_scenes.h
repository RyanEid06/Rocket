#pragma once

#include "image_io.h"

#include <string>
#include <vector>

namespace rocket3::visual_compare {

struct CanonicalScene {
    std::string name;
    ImageRgba image;
    std::vector<std::string> coverage;
    bool portability_subset = false;
};

std::vector<CanonicalScene> render_canonical_scenes();

} // namespace rocket3::visual_compare
