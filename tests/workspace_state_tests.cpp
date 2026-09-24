#include "dependency_graph.h"
#include "test_support.h"
#include "workspace_state.h"

#include <filesystem>

int main() {
  int failures = 0;
  rocket::WorkspaceState state;
  const std::filesystem::path path = "C:/workspace/interface.rocket";
  const std::string publicDeclaration = "pub struct Pair[T]:\n    first: Int\n";
  const auto &parsed = state.parse(path, publicDeclaration);
  rocket::test::expect(&parsed == &state.parse(path, publicDeclaration),
                       "identical source retains its parsed owner", failures);
  rocket::test::expect(
      parsed.identity == std::filesystem::absolute(path).lexically_normal(),
      "parsed owner uses normalized module identity", failures);
  const auto original = parsed.exportedInterface;
  const auto originalHash = parsed.interfaceHash;
  const auto &privateParsed = state.parse(
      path, publicDeclaration + "fn private() -> Int:\n    return 1\n");
  const auto privateBody = privateParsed.exportedInterface;
  const auto privateHash = privateParsed.interfaceHash;
  const auto &changed =
      state.parse(path, "pub struct Pair[U]:\n    first: Int\n"
                        "fn private() -> Int:\n    return 1\n");
  const auto publicChange = changed.exportedInterface;
  rocket::test::expect(original == privateBody,
                       "private body leaves the exported interface unchanged",
                       failures);
  rocket::test::expect(
      originalHash == privateHash,
      "private body leaves the interface fingerprint unchanged", failures);
  rocket::test::expect(original != publicChange,
                       "public type parameter changes the interface record",
                       failures);
  rocket::test::expect(originalHash != changed.interfaceHash,
                       "public type parameter changes the interface hash",
                       failures);
  for (int index = 0; index < 500; ++index) {
    const auto transient = std::filesystem::path("C:/workspace/transient-") /
                           (std::to_string(index) + ".rocket");
    state.parse(transient, "fn value() -> Int:\n    return 1\n");
    state.erase(transient);
  }
  rocket::test::expect(
      state.parsedSourceCount() == 1,
      "closed rootless sources do not accumulate parsed owners", failures);
  rocket::DependencyGraph graph;
  const auto root = std::filesystem::path("C:/workspace/root.rocket");
  rocket::test::expect(graph.recordRoot(root, {path.generic_string()}),
                       "first root records dependency ownership", failures);
  rocket::test::expect(!graph.recordRoot(root, {path.generic_string()}),
                       "unchanged dependency ownership avoids cache pruning",
                       failures);
  rocket::test::expect(graph.recordRoot(root, {}),
                       "removed import changes dependency ownership", failures);
  return failures == 0 ? 0 : 1;
}
