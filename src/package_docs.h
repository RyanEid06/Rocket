#pragma once

#include "package.h"

#include <filesystem>
#include <string>

namespace rocket {

bool generatePackageDocumentation(const Package& package,
                                  const std::filesystem::path& outputDirectory,
                                  std::string& report, std::string& error);

} // namespace rocket
