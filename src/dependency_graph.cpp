#include "dependency_graph.h"

namespace rocket {
namespace {
std::filesystem::path normalized(const std::filesystem::path &path) {
  return std::filesystem::absolute(path).lexically_normal();
}
} // namespace

bool DependencyGraph::recordRoot(const std::filesystem::path &root,
                                 const std::set<std::string> &loaded) {
  const auto key = normalized(root);
  std::set<std::filesystem::path> dependencies;
  dependencies.insert(key);
  for (const auto &path : loaded)
    dependencies.insert(normalized(path));
  const auto prior = roots_.find(key);
  if (prior != roots_.end() && prior->second == dependencies)
    return false;
  removeRoot(key);
  roots_[key] = dependencies;
  for (const auto &dependency : dependencies)
    reverse_[dependency].insert(key);
  return true;
}

void DependencyGraph::removeRoot(const std::filesystem::path &root) {
  const auto key = normalized(root);
  const auto prior = roots_.find(key);
  if (prior == roots_.end())
    return;
  for (const auto &dependency : prior->second) {
    auto edge = reverse_.find(dependency);
    if (edge == reverse_.end())
      continue;
    edge->second.erase(key);
    if (edge->second.empty())
      reverse_.erase(edge);
  }
  roots_.erase(prior);
}

std::set<std::filesystem::path> DependencyGraph::affectedRoots(
    const std::set<std::filesystem::path> &changed) const {
  std::set<std::filesystem::path> affected;
  for (const auto &path : changed) {
    const auto key = normalized(path);
    if (const auto found = reverse_.find(key); found != reverse_.end())
      affected.insert(found->second.begin(), found->second.end());
    if (roots_.contains(key))
      affected.insert(key);
  }
  return affected;
}

std::set<std::filesystem::path> DependencyGraph::loadedSources() const {
  std::set<std::filesystem::path> sources;
  for (const auto &[source, roots] : reverse_)
    sources.insert(source);
  return sources;
}

} // namespace rocket
