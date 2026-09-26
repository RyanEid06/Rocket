#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>

namespace rocket {

// Records actual compiler-loaded closure for each root. Invalidation follows
// reverse edges, including transitive imports observed by the loader.
class DependencyGraph {
public:
  bool recordRoot(const std::filesystem::path &root,
                  const std::set<std::string> &loaded);
  void removeRoot(const std::filesystem::path &root);
  std::set<std::filesystem::path>
  affectedRoots(const std::set<std::filesystem::path> &changed) const;
  std::set<std::filesystem::path> loadedSources() const;
  void clear() {
    reverse_.clear();
    roots_.clear();
  }

private:
  std::map<std::filesystem::path, std::set<std::filesystem::path>> roots_;
  std::map<std::filesystem::path, std::set<std::filesystem::path>> reverse_;
};

} // namespace rocket
