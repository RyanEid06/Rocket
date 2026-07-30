#pragma once

#include "ast.h"

#include <string>

namespace rocket {

// Emits the deterministic public C contract for Rocket exports. The header is
// intentionally limited to the Phase 13 primitive/pointer ABI.
bool generateNativeHeader(const Module& module, const std::string& packageName,
                          std::string& output, std::string& error);

// Translates the deterministic Phase 13 C-header subset into low-level Rocket
// extern declarations. Unsupported declarations are rejected instead of being
// guessed.
bool generateRocketBindings(const std::string& header, std::string& output,
                            std::string& error);

} // namespace rocket
