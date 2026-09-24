#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "document_structure.h"

#include <filesystem>
#include <map>
#include <memory>
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
  void noteVersion(const std::filesystem::path &path, long long version);
  void clear() {
    parsed_.clear();
    versions_.clear();
  }

private:
  std::map<std::filesystem::path, std::shared_ptr<const ParsedSource>> parsed_;
  std::map<std::filesystem::path, long long> versions_;
};

} // namespace rocket
