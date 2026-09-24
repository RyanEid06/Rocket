#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "document_structure.h"

#include <filesystem>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

namespace rocket {

struct ParsedSource {
  std::filesystem::path identity;
  std::string source;
  std::size_t hash = 0;
  std::string exportedInterface;
  std::size_t interfaceHash = 0;
  std::vector<Token> tokens;
  Module module;
  DocumentStructure structure;
  Diagnostics diagnostics;
};

// Worker-owned compiler boundary. A parsed tree is immutable after insertion;
// loader rewriting always receives a deep copy. Source comparison accompanies
// the hash so collisions cannot reuse a different parse.
class WorkspaceState {
public:
  const ParsedSource &parse(const std::filesystem::path &path,
                            const std::string &source);
  const ParsedSource *find(const std::filesystem::path &path) const;
  void erase(const std::filesystem::path &path);
  void retain(const std::set<std::filesystem::path> &sources);
  std::size_t parsedSourceCount() const { return parsed_.size(); }
  void clear() { parsed_.clear(); }

private:
  std::map<std::filesystem::path, std::shared_ptr<const ParsedSource>> parsed_;
};

} // namespace rocket
