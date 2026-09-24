#include "document_structure.h"
#include "analysis_control.h"

#include <algorithm>
#include <unordered_map>

namespace rocket {
namespace {

std::string shortName(const std::string &name) {
  const auto dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

struct StructureLookup {
  std::vector<int> lineLengths{0};
  std::unordered_map<int, std::vector<const Token *>> tokensByLine;
  std::unordered_map<int, int> foldEndByStart;

  int lineLength(int line) const {
    return line > 0 && static_cast<std::size_t>(line) <= lineLengths.size()
               ? lineLengths[static_cast<std::size_t>(line - 1)]
               : 0;
  }

  Location nameLocation(const Location &start, const std::string &name) const {
    const auto found = tokensByLine.find(start.line);
    if (found == tokensByLine.end())
      return start;
    for (const Token *token : found->second) {
      analysisCheckpoint();
      if (token->location.column >= start.column &&
          token->kind == TokenKind::Identifier && token->text == name)
        return token->location;
    }
    return start;
  }
};

StructureSymbol symbol(const StructureLookup &lookup, const Location &location,
                       std::string name, long long kind) {
  const Location selected = lookup.nameLocation(location, name);
  int endLine = location.line;
  if (const auto fold = lookup.foldEndByStart.find(location.line);
      fold != lookup.foldEndByStart.end())
    endLine = std::max(endLine, fold->second);
  StructureSymbol result;
  result.name = std::move(name);
  result.kind = kind;
  result.range = {location,
                  {location.file, endLine, lookup.lineLength(endLine) + 1}};
  result.selection = {selected,
                      {selected.file, selected.line,
                       selected.column + static_cast<int>(result.name.size())}};
  return result;
}

} // namespace

DocumentStructure buildDocumentStructure(const Module &module,
                                         const std::vector<Token> &tokens,
                                         const std::string &source) {
  DocumentStructure result;
  StructureLookup lookup;
  for (std::size_t index = 0; index < source.size(); ++index) {
    if (source[index] == '\n') {
      if (index > 0 && source[index - 1] == '\r')
        --lookup.lineLengths.back();
      lookup.lineLengths.push_back(0);
    } else
      ++lookup.lineLengths.back();
  }
  if (!source.empty() && source.back() == '\r')
    --lookup.lineLengths.back();
  std::vector<int> starts;
  for (const auto &token : tokens) {
    analysisCheckpoint();
    lookup.tokensByLine[token.location.line].push_back(&token);
    if (token.kind == TokenKind::Indent)
      starts.push_back(std::max(0, token.location.line - 2));
    else if (token.kind == TokenKind::Dedent && !starts.empty()) {
      const int start = starts.back();
      starts.pop_back();
      const int end = token.location.line - 2;
      if (end > start)
        result.folds.push_back({start, end});
    }
  }
  std::sort(result.folds.begin(), result.folds.end(),
            [](const auto &left, const auto &right) {
              return left.startLine == right.startLine
                         ? left.endLine < right.endLine
                         : left.startLine < right.startLine;
            });
  for (const auto &fold : result.folds) {
    analysisCheckpoint();
    auto &end = lookup.foldEndByStart[fold.startLine + 1];
    end = std::max(end, fold.endLine + 1);
  }
  for (const auto &item : module.structs) {
    analysisCheckpoint();
    auto parent = symbol(lookup, item.location, item.name, 23);
    for (const auto &field : item.fields)
      parent.children.push_back(symbol(lookup, field.location, field.name, 8));
    result.symbols.push_back(std::move(parent));
  }
  for (const auto &item : module.enums) {
    analysisCheckpoint();
    auto parent = symbol(lookup, item.location, item.name, 10);
    for (const auto &variant : item.variants)
      parent.children.push_back(
          symbol(lookup, variant.location, variant.name, 22));
    result.symbols.push_back(std::move(parent));
  }
  for (const auto &item : module.traits) {
    analysisCheckpoint();
    auto parent = symbol(lookup, item.location, item.name, 11);
    for (const auto &method : item.methods)
      parent.children.push_back(
          symbol(lookup, method.location, method.name, 6));
    result.symbols.push_back(std::move(parent));
  }
  for (const auto &item : module.functions) {
    analysisCheckpoint();
    result.symbols.push_back(symbol(lookup, item.location, shortName(item.name),
                                    item.associatedConstant    ? 14
                                    : item.methodOwner.empty() ? 12
                                                               : 6));
  }
  std::sort(result.symbols.begin(), result.symbols.end(),
            [](const auto &left, const auto &right) {
              if (left.range.start.line != right.range.start.line)
                return left.range.start.line < right.range.start.line;
              return left.range.start.column < right.range.start.column;
            });
  return result;
}

} // namespace rocket
