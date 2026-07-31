#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "package.h"

#include <filesystem>
#include <optional>

namespace rocket {

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

} // namespace rocket
