#pragma once

#include "hir.h"

#include <optional>
#include <string>

namespace rocket {

class SemanticAnalyzer {
public:
  SemanticAnalyzer(const Module& module, Diagnostics& diagnostics)
      : module_(module), diagnostics_(diagnostics) {}
  bool analyze();
  std::optional<HirModule> analyzeToHir();

  static Type typeFromName(const std::string& name) { return rocket::typeFromName(name); }
  static const char* typeName(Type type) { return rocket::typeName(type); }

private:
  const Module& module_;
  Diagnostics& diagnostics_;
};

} // namespace rocket
