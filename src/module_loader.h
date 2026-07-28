#pragma once

#include "ast.h"
#include "diagnostic.h"

#include <filesystem>
#include <optional>

namespace rocket {

// Loads the root file and its relative imports, diagnoses cycles and visibility
// violations, then returns one deterministically namespaced AST for HIR.
std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      Diagnostics& diagnostics);

} // namespace rocket
