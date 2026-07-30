#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rocket {

enum class PackageOutputKind { Executable, StaticLibrary, DynamicLibrary };

struct Package {
  std::filesystem::path root;
  std::string name;
  std::string version;
  std::filesystem::path entry;
  std::filesystem::path tests;
  PackageOutputKind outputKind = PackageOutputKind::Executable;
  std::string outputName;
  std::vector<std::string> nativeLibraries;
  std::vector<std::filesystem::path> nativeLibrarySearch;
  std::vector<std::filesystem::path> nativeHeaders;
};

std::optional<Package> loadPackage(const std::filesystem::path& path,
                                   std::string& error);
bool createPackage(const std::filesystem::path& directory,
                   const std::string& requestedName, std::string& error);
std::vector<std::filesystem::path> packageTests(const Package& package,
                                                std::string& error);
std::vector<std::filesystem::path> rocketSources(const std::filesystem::path& path,
                                                 std::string& error);

} // namespace rocket
