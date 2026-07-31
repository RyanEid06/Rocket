#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "package.h"

#include <filesystem>
#include <map>
#include <optional>
#include <string>

namespace rocket {

// Absolute, lexically-normal source paths mapped to unsaved UTF-8 contents.
// Tooling uses overlays to run the ordinary module/HIR pipeline without
// writing editor buffers to disk. The loader never executes package code or
// performs transport while consulting this map.
using SourceOverlays = std::map<std::filesystem::path, std::string>;

void setStandardLibraryRoot(std::filesystem::path root);

// Loads the root file and its relative imports, diagnoses cycles and visibility
// violations, then returns one deterministically namespaced AST for HIR.
std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      Diagnostics& diagnostics);
std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      const std::filesystem::path& packageRoot,
                                      Diagnostics& diagnostics);
std::optional<Module> loadModuleGraph(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& packageRoot,
    const std::vector<PackageDependencyRoot>& dependencyRoots,
    Diagnostics& diagnostics);
std::optional<Module> loadModuleGraph(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& packageRoot,
    const std::vector<PackageDependencyRoot>& dependencyRoots,
    const SourceOverlays& overlays, Diagnostics& diagnostics);

} // namespace rocket
