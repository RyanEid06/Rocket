#include "sema.h"

namespace rocket {

bool SemanticAnalyzer::analyze() { return analyzeToHir().has_value(); }

std::optional<HirModule> SemanticAnalyzer::analyzeToHir() {
  HirLowerer lowerer(module_, diagnostics_);
  return lowerer.lower();
}

} // namespace rocket
