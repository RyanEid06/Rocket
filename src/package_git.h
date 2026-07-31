#pragma once

#include <filesystem>
#include <string>

namespace rocket {

struct GitAcquisition {
  std::filesystem::path sourceRoot;
  std::filesystem::path temporaryRoot;
};

bool acquireGitPackage(const std::string& url, const std::string& revision,
                       const std::filesystem::path& stagingRoot,
                       GitAcquisition& acquisition, std::string& error);
void discardGitAcquisition(const GitAcquisition& acquisition);

} // namespace rocket
