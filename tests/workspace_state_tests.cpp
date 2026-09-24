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
  const auto privateBody =
      state
          .parse(path,
                 publicDeclaration + "fn private() -> Int:\n    return 1\n")
          .exportedInterface;
  const auto &changed =
      state.parse(path, "pub struct Pair[U]:\n    first: Int\n"
                        "fn private() -> Int:\n    return 1\n");
  const auto publicChange = changed.exportedInterface;
  rocket::test::expect(original == privateBody,
                       "private body leaves the exported interface unchanged",
                       failures);
  rocket::test::expect(original != publicChange,
                       "public type parameter changes the interface record",
                       failures);
  rocket::test::expect(originalHash != changed.interfaceHash,
                       "public type parameter changes the interface hash",
                       failures);
  return failures == 0 ? 0 : 1;
}
