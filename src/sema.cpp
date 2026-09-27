#include "sema.h"

namespace rocket {

bool SemanticAnalyzer::analyze() { return analyzeToHir().has_value(); }

std::optional<HirModule>
SemanticAnalyzer::analyzeToHir(const std::set<std::string> *bodySources) {
  HirLowerer lowerer(module_, diagnostics_, bodySources);
  return lowerer.lower();
}

} // namespace rocket
