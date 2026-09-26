#pragma once

#include "ast.h"

#include <string>
#include <vector>

namespace rocket {

struct StructureRange {
  Location start;
  Location end;
};
struct StructureSymbol {
  std::string name;
  long long kind = 12;
  StructureRange range;
  StructureRange selection;
  std::vector<StructureSymbol> children;
};
struct FoldingSpan {
  int startLine = 0;
  int endLine = 0;
};
struct DocumentStructure {
  std::vector<StructureSymbol> symbols;
  std::vector<FoldingSpan> folds;
};

// All declarations come from the parser AST. Folding extents use lexer
// Indent/Dedent tokens, which are the parser's actual block boundaries.
DocumentStructure buildDocumentStructure(const Module &module,
                                         const std::vector<Token> &tokens,
                                         const std::string &source);

} // namespace rocket
