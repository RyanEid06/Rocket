#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  rocket::Diagnostics diagnostics;
  auto mir = rocket::test::lowerToMir(
      "fn enabled() -> Bool:\n"
      "    return true\n"
      "fn limit() -> Int:\n"
      "    return 3\n"
      "fn main() -> Int:\n"
      "    var total = 0\n"
      "    for index in 0..limit():\n"
      "        if enabled() and index == 1:\n"
      "            continue\n"
      "        total = total + index\n"
      "    while total < 5:\n"
      "        total = total + 1\n"
      "        break\n"
      "    return total\n",
      diagnostics);
  rocket::test::expect(mir.has_value(), "valid typed HIR lowers to MIR", failures);
  if (mir.has_value()) {
    std::string verifierError;
    rocket::test::expect(rocket::verifyMir(*mir, verifierError),
                         "lowered MIR satisfies structural and type invariants", failures);
    const std::string dump = rocket::dumpMir(*mir);
    rocket::test::expect(dump.find("call @enabled") != std::string::npos,
                         "MIR calls retain resolved function declarations", failures);
    rocket::test::expect(dump.find("branch") != std::string::npos &&
                             dump.find("goto") != std::string::npos,
                         "loops and conditions lower to explicit control-flow edges", failures);
    rocket::test::expect(dump.find(" and ") == std::string::npos,
                         "short-circuit operators lower to branches rather than eager operations",
                         failures);
    const std::size_t firstLimitCall = dump.find("call @limit");
    rocket::test::expect(firstLimitCall != std::string::npos &&
                             dump.find("call @limit", firstLimitCall + 1) == std::string::npos,
                         "range end expressions are evaluated exactly once before iteration",
                         failures);

    auto invalid = *mir;
    invalid.functions[0].blocks[0].terminator->target =
        static_cast<rocket::MirBlockId>(invalid.functions[0].blocks.size() + 1);
    invalid.functions[0].blocks[0].terminator->kind = rocket::MirTerminatorKind::Goto;
    rocket::test::expect(!rocket::verifyMir(invalid, verifierError),
                         "MIR verifier rejects invalid block targets", failures);
  }
  return rocket::test::finish(failures, "mir");
}
