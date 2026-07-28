#pragma once

#include "diagnostic.h"

#include <optional>
#include <string>

namespace rocket {

// Formats lexically valid Rocket source using four-space indentation, canonical
// token spacing, LF newlines, and one final newline. Comments are preserved.
std::optional<std::string> formatSource(const std::string& file,
                                        const std::string& source,
                                        Diagnostics& diagnostics);

} // namespace rocket
