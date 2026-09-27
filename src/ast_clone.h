#pragma once

#include "ast.h"

namespace rocket {

// Parser trees own their children. The loader rewrites names and moves nodes
// into a flattened graph, so a cached parse tree must be copied before use.
Module cloneModule(const Module &source);

} // namespace rocket
