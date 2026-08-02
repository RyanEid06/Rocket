#include "lexer.h"
#include "module_loader.h"
#include "package.h"
#include "parser.h"
#include "test_support.h"

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace {

std::uint64_t nextRandom(std::uint64_t& state) {
  state ^= state << 13;
  state ^= state >> 7;
  state ^= state << 17;
  return state;
}

std::string fuzzInput(std::uint64_t& state, std::size_t maximum) {
  static constexpr char alphabet[] =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ \t\r\n"
      "()[]:,.?+-*/=!<>#\\\"'@$%&|{};";
  const std::size_t length =
      static_cast<std::size_t>(nextRandom(state) % (maximum + 1));
  std::string result;
  result.reserve(length);
  for (std::size_t index = 0; index < length; ++index) {
    const auto selected = static_cast<std::size_t>(
        nextRandom(state) % (sizeof(alphabet) - 1));
    result.push_back(alphabet[selected]);
  }
  return result;
}

std::string frontendSnapshot(const std::string& source) {
  rocket::Diagnostics diagnostics;
  rocket::Lexer lexer("fuzz.rocket", source, diagnostics);
  const auto tokens = lexer.lex();
  rocket::Parser parser(tokens, diagnostics);
  const auto module = parser.parseModule();
  std::ostringstream snapshot;
  for (const auto& token : tokens) {
    snapshot << static_cast<int>(token.kind) << ':' << token.location.line << ':'
             << token.location.column << ':' << token.text.size() << ':'
             << token.text << '\n';
  }
  snapshot << "ast=" << module.imports.size() << ',' << module.structs.size()
           << ',' << module.enums.size() << ',' << module.traits.size() << ','
           << module.functions.size() << '\n';
  for (const auto& diagnostic : diagnostics.all()) {
    snapshot << rocket::diagnosticCodeName(diagnostic.code) << ':'
             << diagnostic.location.line << ':' << diagnostic.location.column
             << ':' << diagnostic.message << '\n';
  }
  return snapshot.str();
}

bool write(const std::filesystem::path& path, const std::string& contents) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output << contents;
  return static_cast<bool>(output);
}

int boundedEnvironment(const char* name, int fallback, int maximum) {
  const char* raw = std::getenv(name);
  if (raw == nullptr) return fallback;
  try {
    const int value = std::stoi(raw);
    return value >= 1 && value <= maximum ? value : fallback;
  } catch (...) {
    return fallback;
  }
}

} // namespace

int main() {
  int failures = 0;
  constexpr std::uint64_t seed = 0x524f434b45543230ULL;
  std::uint64_t state = seed;
  const int frontendCases =
      boundedEnvironment("ROCKET_HARDENING_FRONTEND_CASES", 128, 10000);
  for (int iteration = 0; iteration < frontendCases; ++iteration) {
    const std::string source = fuzzInput(state, 128);
    const std::string first = frontendSnapshot(source);
    const std::string second = frontendSnapshot(source);
    if (first != second) {
      rocket::test::expect(false,
                           "frontend fuzz case is deterministic at iteration " +
                               std::to_string(iteration),
                           failures);
      break;
    }
  }

  for (int iteration = 0; iteration < 32; ++iteration) {
    const std::string source =
        "fn helper(value: Int) -> Int:\n"
        "    return value + " + std::to_string(iteration) + "\n"
        "fn main() -> Int:\n"
        "    let value = helper(" + std::to_string(iteration % 17) + ")\n"
        "    if value >= 0:\n"
        "        return 0\n"
        "    return 1\n";
    rocket::Diagnostics diagnostics;
    const auto mir = rocket::test::lowerToMir(source, diagnostics);
    std::string verifierError;
    if (!mir || diagnostics.hasErrors() ||
        !rocket::verifyMir(*mir, verifierError)) {
      rocket::test::expect(false,
                           "generated valid frontend case reaches verified MIR",
                           failures);
      break;
    }
  }

  const auto overlayPath =
      (std::filesystem::current_path() / "phase20-oversized.rocket")
          .lexically_normal();
  rocket::SourceOverlays overlays;
  overlays.emplace(overlayPath, std::string(4U * 1024U * 1024U + 1U, 'x'));
  rocket::Diagnostics limitDiagnostics;
  const auto limited = rocket::loadModuleGraph(
      overlayPath, overlayPath.parent_path(), {}, overlays, limitDiagnostics);
  rocket::test::expect(
      !limited && limitDiagnostics.hasErrors() &&
          limitDiagnostics.all().front().code ==
              rocket::DiagnosticCode::ResourceLimit,
      "oversized source overlays fail with stable R1003", failures);

  const auto depthRoot =
      (std::filesystem::current_path() / "phase20-depth-root.rocket")
          .lexically_normal();
  rocket::SourceOverlays depthOverlays;
  depthOverlays.emplace(depthRoot, "import m0\n");
  for (int index = 0; index < 64; ++index) {
    const auto path = (depthRoot.parent_path() /
                       ("m" + std::to_string(index) + ".rocket"))
                          .lexically_normal();
    depthOverlays.emplace(
        path, index == 63
                  ? "fn value() -> Int:\n    return 0\n"
                  : "import m" + std::to_string(index + 1) + "\n");
  }
  rocket::Diagnostics depthDiagnostics;
  const auto deeplyNested = rocket::loadModuleGraph(
      depthRoot, depthRoot.parent_path(), {}, depthOverlays, depthDiagnostics);
  bool depthLimited = false;
  for (const auto& diagnostic : depthDiagnostics.all())
    depthLimited = depthLimited ||
                   diagnostic.code == rocket::DiagnosticCode::ResourceLimit;
  rocket::test::expect(!deeplyNested && depthLimited,
                       "deep import chains fail at the stable nesting bound",
                       failures);

  const auto manifestRoot =
      std::filesystem::current_path() / "phase20_hardening_manifest";
  std::error_code filesystemError;
  std::filesystem::remove_all(manifestRoot, filesystemError);
  std::filesystem::create_directories(manifestRoot / "src", filesystemError);
  write(manifestRoot / "src/main.rocket",
        "fn main() -> Int:\n    return 0\n");
  write(manifestRoot / "rocket.toml", std::string(1024U * 1024U + 1U, '#'));
  std::string manifestError;
  rocket::test::expect(
      !rocket::loadPackage(manifestRoot, manifestError) &&
          manifestError.find("1 MiB") != std::string::npos,
      "oversized package manifests are rejected before parsing", failures);

  state = seed ^ 0xa5a5a5a5a5a5a5a5ULL;
  const int manifestCases =
      boundedEnvironment("ROCKET_HARDENING_MANIFEST_CASES", 64, 4096);
  for (int iteration = 0; iteration < manifestCases; ++iteration) {
    write(manifestRoot / "rocket.toml", fuzzInput(state, 1024));
    manifestError.clear();
    (void)rocket::loadPackage(manifestRoot, manifestError);
  }
  std::filesystem::remove_all(manifestRoot, filesystemError);

  if (failures == 0) {
    std::cout << "phase20 hardening seed=" << seed
              << " frontend-cases=" << frontendCases
              << " manifest-cases=" << manifestCases << '\n';
  }
  return rocket::test::finish(failures, "hardening");
}
