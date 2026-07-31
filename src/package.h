#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace rocket {

enum class PackageOutputKind { Executable, StaticLibrary, DynamicLibrary };

enum class DependencySourceKind { Registry, Git, Path };

struct PackageDependency {
  std::string name;
  DependencySourceKind source = DependencySourceKind::Registry;
  std::string requirement;
  std::string location;
  std::string revision;
};

struct Package {
  std::filesystem::path root;
  std::string name;
  std::string version;
  std::string license;
  std::string registry;
  std::filesystem::path entry;
  std::filesystem::path tests;
  PackageOutputKind outputKind = PackageOutputKind::Executable;
  std::string outputName;
  std::vector<std::string> nativeLibraries;
  std::vector<std::filesystem::path> nativeLibrarySearch;
  std::vector<std::filesystem::path> nativeHeaders;
  std::vector<PackageDependency> dependencies;
  std::string namespaceName = "local";
  std::string registryKey;
  std::vector<std::string> allowedLicenses;
  std::vector<std::string> allowedNativeDependencies;
  bool denyYanked = false;
  std::string buildScripts = "deny";
  std::string buildScript;
};

struct LockedPackage {
  std::string name;
  std::string version;
  std::string source;
  std::string checksum;
  std::string license;
  std::vector<std::string> dependencies;
  std::string namespaceName = "local";
  std::string registryKey;
  std::string publisher;
  bool yanked = false;
};

struct PackageLock {
  std::string rootName;
  std::string rootVersion;
  std::vector<std::string> rootDependencies;
  std::vector<LockedPackage> packages;
  int formatVersion = 1;
  std::string rootNamespace = "local";
};

struct PackageDependencyRoot {
  std::string name;
  std::string identity;
  std::filesystem::path root;
  std::filesystem::path entry;
  std::vector<std::string> dependencies;
  bool direct = false;
  std::vector<std::string> nativeLibraries;
  std::vector<std::filesystem::path> nativeLibrarySearch;
  std::vector<std::filesystem::path> nativeHeaders;
};

struct ResolveOptions {
  bool offline = false;
  bool locked = false;
};

std::optional<Package> loadPackage(const std::filesystem::path& path,
                                   std::string& error);
bool createPackage(const std::filesystem::path& directory,
                   const std::string& requestedName, std::string& error);
std::vector<std::filesystem::path> packageTests(const Package& package,
                                                std::string& error);
std::vector<std::filesystem::path> rocketSources(const std::filesystem::path& path,
                                                 std::string& error);

bool isValidSemanticVersion(const std::string& version);
bool semanticVersionSatisfies(const std::string& version,
                              const std::string& requirement);
int compareSemanticVersionText(const std::string& left,
                               const std::string& right);
bool resolvePackageDependencies(const Package& package,
                                const ResolveOptions& options,
                                PackageLock& lock, std::string& error);
bool readPackageLock(const std::filesystem::path& path, PackageLock& lock,
                     std::string& error);
bool writePackageLock(const std::filesystem::path& path, const PackageLock& lock,
                      std::string& error);
std::string packageDependencyTree(const PackageLock& lock);
bool auditPackageDependencies(const Package& package, const PackageLock& lock,
                              std::string& report, std::string& error);
bool prepareLockedPackageDependencies(
    const Package& package, bool offline,
    std::vector<PackageDependencyRoot>& roots, PackageLock& lock,
    std::string& error);
bool packageSourceChecksum(const std::filesystem::path& root,
                           std::string& checksum, std::string& error);

} // namespace rocket
