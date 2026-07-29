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

  rocket::Diagnostics ownershipDiagnostics;
  auto ownershipMir = rocket::test::lowerToMir(
      "fn identity(value: String) -> String:\n"
      "    return value\n"
      "fn main() -> Int:\n"
      "    var first = \"rocket\"\n"
      "    let alias = first\n"
      "    first = identity(alias)\n"
      "    print(first)\n"
      "    return 0\n",
      ownershipDiagnostics);
  rocket::test::expect(ownershipMir.has_value(), "managed String source lowers to MIR", failures);
  if (ownershipMir.has_value()) {
    std::string verifierError;
    rocket::test::expect(rocket::verifyMir(*ownershipMir, verifierError),
                         "ARC-annotated MIR verifies: " + verifierError, failures);
    const std::string dump = rocket::dumpMir(*ownershipMir);
    rocket::test::expect(dump.find("retain") != std::string::npos &&
                             dump.find("release") != std::string::npos,
                         "MIR makes managed copies and cleanup explicit", failures);
  }


  rocket::Diagnostics collectionDiagnostics;
  auto collectionMir = rocket::test::lowerToMir(
      "fn main() -> Int:\n"
      "    let values = [10, 20, 30]\n"
      "    let tail = values[1..3]\n"
      "    print(tail[0])\n"
      "    return 0\n",
      collectionDiagnostics);
  rocket::test::expect(collectionMir.has_value(), "collections lower to MIR", failures);
  if (collectionMir.has_value()) {
    std::string verifierError;
    rocket::test::expect(rocket::verifyMir(*collectionMir, verifierError),
                         "collection MIR verifies: " + verifierError, failures);
    const std::string dump = rocket::dumpMir(*collectionMir);
    rocket::test::expect(dump.find("array 10 20 30") != std::string::npos &&
                             dump.find("slice") != std::string::npos &&
                             dump.find("index") != std::string::npos,
                         "MIR represents construction, slicing, and indexing explicitly",
                         failures);
    rocket::test::expect(dump.find("release") != std::string::npos,
                         "Array and Slice values participate in MIR ARC", failures);
  }
  rocket::Diagnostics mutationDiagnostics;
  auto mutationMir = rocket::test::lowerToMir(
      "fn main() -> Int:\n"
      "    var values = [10, 20]\n"
      "    let alias = values\n"
      "    values[0] = 30\n"
      "    return values[0]\n",
      mutationDiagnostics);
  rocket::test::expect(mutationMir.has_value(), "Array mutation lowers to MIR", failures);
  if (mutationMir.has_value()) {
    std::string verifierError;
    rocket::test::expect(rocket::verifyMir(*mutationMir, verifierError),
                         "Array mutation MIR verifies: " + verifierError, failures);
    const std::string dump = rocket::dumpMir(*mutationMir);
    rocket::test::expect(dump.find("array-update") != std::string::npos,
                         "MIR explicitly represents copy-on-write Array updates", failures);
  }
  rocket::Diagnostics phase6Diagnostics;
  auto phase6Mir = rocket::test::lowerToMir(
      "struct Pair:\n"
      "    first: Int\n"
      "    second: Int\n"
      "fn main() -> Int:\n"
      "    let pair = Pair(3, 4)\n"
      "    let maybe: Option[Int] = Some(pair.second)\n"
      "    match maybe:\n"
      "        case Some(value):\n"
      "            return value\n"
      "        case None:\n"
      "            return 0\n",
      phase6Diagnostics);
  rocket::test::expect(phase6Mir.has_value(), "aggregates and match lower to MIR", failures);
  if (phase6Mir.has_value()) {
    std::string verifierError;
    rocket::test::expect(rocket::verifyMir(*phase6Mir, verifierError),
                         "Phase 6 MIR verifies: " + verifierError, failures);
    const std::string dump = rocket::dumpMir(*phase6Mir);
    rocket::test::expect(dump.find("aggregate @Pair#0") != std::string::npos &&
                             dump.find("tag") != std::string::npos &&
                             dump.find("field") != std::string::npos,
                         "MIR explicitly represents aggregate construction and matching",
                         failures);
  }
  return rocket::test::finish(failures, "mir");
}
