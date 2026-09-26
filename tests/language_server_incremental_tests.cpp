#include "lsp_session.h"
#include "test_support.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {
std::string jsonQuoted(const std::string &text) {
  std::string result = "\"";
  for (const char character : text) {
    if (character == '\\' || character == '"') {
      result.push_back('\\');
      result.push_back(character);
    } else if (character == '\n')
      result += "\\n";
    else if (character == '\r')
      result += "\\r";
    else
      result.push_back(character);
  }
  return result + '"';
}
std::string uri(const std::string &name) {
  return "file:///C:/workspace/" + name + ".rocket";
}
std::string fileUri(const std::filesystem::path &path) {
  const std::string generic = path.generic_string();
  std::string result = "file://";
  if (generic.empty() || generic.front() != '/')
    result.push_back('/');
  for (const char character : generic) {
    if (character == ' ')
      result += "%20";
    else
      result.push_back(character);
  }
  return result;
}
void initialize(rocket::test::LspSession &session) {
  session.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  session.response("init");
}
void open(rocket::test::LspSession &session, const std::string &name,
          const std::string &source, int version) {
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
      jsonQuoted(uri(name)) + R"(,"version":)" + std::to_string(version) +
      R"(,"text":)" + jsonQuoted(source) + R"(}}})");
  if (source.size() <= 500000) {
    session.settle();
  } else {
    constexpr int settleSeconds = 180;
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(settleSeconds);
    while (std::chrono::steady_clock::now() < deadline) {
      session.send(
          R"({"jsonrpc":"2.0","id":"large-barrier","method":"rocket/projectStatus"})");
      if (session.response("large-barrier").find("\"analysisPending\":true") ==
          std::string::npos)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("large source analysis did not settle in " +
                             std::to_string(settleSeconds) + "s");
  }
}
void change(rocket::test::LspSession &session, const std::string &name,
            const std::string &source) {
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":)" +
      jsonQuoted(uri(name)) + R"(,"version":2},"contentChanges":[{"text":)" +
      jsonQuoted(source) + R"(}]}})");
  session.settle();
}
std::string latestDiagnostics(const std::string &recorded,
                              const std::string &documentUri) {
  std::string latest;
  std::size_t start = 0;
  while (start < recorded.size()) {
    const auto header = recorded.find("\r\n\r\n", start);
    if (header == std::string::npos)
      break;
    const auto length =
        std::stoull(recorded.substr(start + 16, header - start - 16));
    const auto body = recorded.substr(header + 4, length);
    if (body.find(R"("method":"textDocument/publishDiagnostics")") !=
            std::string::npos &&
        body.find("\"uri\":" + jsonQuoted(documentUri)) != std::string::npos)
      latest = body;
    start = header + 4 + length;
  }
  return latest;
}
std::string request(rocket::test::LspSession &session, const std::string &id,
                    const std::string &method, const std::string &params) {
  session.send(R"({"jsonrpc":"2.0","id":)" + jsonQuoted(id) + R"(,"method":)" +
               jsonQuoted(method) + R"(,"params":)" + params + '}');
  return session.response(id);
}
long long numberField(const std::string &response, const std::string &name) {
  const auto marker = "\"" + name + "\":";
  const auto start = response.find(marker);
  if (start == std::string::npos)
    return -1;
  return std::stoll(response.substr(start + marker.size()));
}
void finish(rocket::test::LspSession &session) {
  session.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  session.response("shutdown");
  session.send(R"({"jsonrpc":"2.0","method":"exit"})");
  session.finish();
}
struct FixtureFile {
  std::string name;
  std::string before;
  std::string after;
};
void compareCleanFull(const std::string &label,
                      const std::vector<FixtureFile> &files, std::size_t edited,
                      int &failures, long long expectedSemanticWork = -1) {
  rocket::test::LspSession incremental;
  initialize(incremental);
  for (const auto &file : files) {
    open(incremental, file.name, file.before, 1);
    if (label == "large unrelated source" && file.name == "large")
      std::cout << "BENCH large cold "
                << request(incremental, "cold", "rocket/projectStatus", "{}")
                << '\n';
  }
  change(incremental, files.at(edited).name, files.at(edited).after);
  if (expectedSemanticWork >= 0) {
    const auto work =
        request(incremental, "strict-work", "rocket/projectStatus", "{}");
    std::cout << "BENCH " << label << " warm " << work << '\n';
    rocket::test::expect(numberField(work, "reparsedFiles") == 1 &&
                             numberField(work, "semanticallyAnalyzedFiles") ==
                                 expectedSemanticWork,
                         label + ": strict incremental semantic work: " + work,
                         failures);
  }
  if (label == "deep chain" || label == "fan in and out") {
    const auto closure =
        request(incremental, "closure", "rocket/projectStatus", "{}");
    const long long expectedContributions = 1;
    rocket::test::expect(
        numberField(closure, "reparsedFiles") == 1 &&
            numberField(closure, "semanticallyAnalyzedFiles") ==
                expectedContributions,
        label +
            ": one reparse and only affected roots contribute semantic work: " +
            closure,
        failures);
  }
  if (label == "one file" || label == "many unrelated modules" ||
      label == "large unrelated source") {
    const auto status =
        request(incremental, "work", "rocket/projectStatus", "{}");
    std::cout << "BENCH " << label << " warm " << status << '\n';
    rocket::test::expect(
        status.find("\"reparsedFiles\":1") != std::string::npos &&
            status.find("\"semanticallyAnalyzedFiles\":1") != std::string::npos,
        label + ": only the edited leaf is parsed and semantically analyzed",
        failures);
#ifdef NDEBUG
    if (label == "large unrelated source")
      rocket::test::expect(
          numberField(status, "elapsedMilliseconds") >= 0 &&
              numberField(status, "elapsedMilliseconds") < 1000,
          "large workspace leaf edit completes in under one second: " + status,
          failures);
#endif
  }
  const auto incrementalDiagnostics = incremental.recorded;
  const auto incrementalSymbols =
      request(incremental, "symbols", "workspace/symbol", R"({"query":""})");
  std::vector<std::string> incrementalDocumentSymbols;
  for (const auto &file : files)
    incrementalDocumentSymbols.push_back(request(
        incremental, "document", "textDocument/documentSymbol",
        R"({"textDocument":{"uri":)" + jsonQuoted(uri(file.name)) + R"(}})"));
  const auto referenceParams =
      R"({"textDocument":{"uri":)" + jsonQuoted(uri("dependency")) +
      R"(},"position":{"line":0,"character":8},"context":{"includeDeclaration":true}})";
  const auto incrementalReferences =
      label == "reference order"
          ? request(incremental, "refs", "textDocument/references",
                    referenceParams)
          : std::string{};
  const auto renameParams =
      R"({"textDocument":{"uri":)" + jsonQuoted(uri("dependency")) +
      R"(},"position":{"line":0,"character":8},"newName":"renamedValue"})";
  const auto incrementalRename =
      label == "reference order"
          ? request(incremental, "rename", "textDocument/rename", renameParams)
          : std::string{};
  finish(incremental);

  rocket::test::LspSession clean;
  initialize(clean);
  for (std::size_t index = 0; index < files.size(); ++index)
    open(clean, files[index].name, files[index].after, index == edited ? 2 : 1);
  const auto cleanDiagnostics = clean.recorded;
  const auto cleanSymbols =
      request(clean, "symbols", "workspace/symbol", R"({"query":""})");
  if (label == "reference order") {
    const auto cleanReferences =
        request(clean, "refs", "textDocument/references", referenceParams);
    rocket::test::expect(incrementalReferences == cleanReferences,
                         label + ": references equal clean full", failures);
    const auto cleanRename =
        request(clean, "rename", "textDocument/rename", renameParams);
    rocket::test::expect(incrementalRename == cleanRename &&
                             cleanRename.find("\"changes\":") !=
                                 std::string::npos,
                         label + ": rename edits equal clean full", failures);
  }
  rocket::test::expect(incrementalSymbols == cleanSymbols,
                       label + ": workspace symbols equal clean full",
                       failures);
  for (std::size_t index = 0; index < files.size(); ++index) {
    const auto cleanDocumentSymbols =
        request(clean, "document", "textDocument/documentSymbol",
                R"({"textDocument":{"uri":)" +
                    jsonQuoted(uri(files[index].name)) + R"(}})");
    rocket::test::expect(
        incrementalDocumentSymbols[index] == cleanDocumentSymbols,
        label + ": document symbols equal clean full for " + files[index].name,
        failures);
    const auto incrementalPublished =
        latestDiagnostics(incrementalDiagnostics, uri(files[index].name));
    const auto cleanPublished =
        latestDiagnostics(cleanDiagnostics, uri(files[index].name));
    rocket::test::expect(
        !incrementalPublished.empty() && !cleanPublished.empty(),
        label + ": both sessions publish diagnostics for " + files[index].name,
        failures);
    rocket::test::expect(incrementalPublished == cleanPublished,
                         label + ": diagnostics equal clean full for " +
                             files[index].name,
                         failures);
  }
  finish(clean);
}

void compareLargeWorkspace(int &failures) {
  constexpr int workspaceFiles = 239;
  std::vector<FixtureFile> files;
  files.push_back({"library", "pub fn value() -> Int:\n    return 1\n",
                   "pub fn value() -> Int:\n    return 2\n"});
  files.push_back(
      {"consumer",
       "import library\nfn use() -> Int:\n    return library.value()\n",
       "import library\nfn use() -> Int:\n    return library.value()\n"});
  for (int index = 0; index < workspaceFiles - 2; ++index) {
    const auto source = "fn value" + std::to_string(index) +
                        "() -> Int:\n    return " + std::to_string(index) +
                        "\n";
    files.push_back({"unrelated" + std::to_string(index), source, source});
  }
  const auto openAll = [&](rocket::test::LspSession &session, bool after) {
    for (const auto &file : files)
      session.send(
          R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
          jsonQuoted(uri(file.name)) + R"(,"version":)" +
          std::to_string(after && file.name == "library" ? 2 : 1) +
          R"(,"text":)" + jsonQuoted(after ? file.after : file.before) +
          R"(}}})");
    const auto deadline =
        std::chrono::steady_clock::now() + std::chrono::seconds(90);
    while (std::chrono::steady_clock::now() < deadline) {
      const auto status =
          request(session, "bulk-status", "rocket/projectStatus", "{}");
      if (status.find("\"analysisPending\":true") == std::string::npos)
        return;
      std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    throw std::runtime_error("239-file workspace did not settle");
  };

  rocket::test::LspSession incremental;
  initialize(incremental);
  openAll(incremental, false);
  change(incremental, "library", files.front().after);
  const auto status =
      request(incremental, "large-work", "rocket/projectStatus", "{}");
  std::cout << "BENCH 239-file private body warm " << status << '\n';
  rocket::test::expect(
      numberField(status, "files") == workspaceFiles &&
          numberField(status, "reparsedFiles") == 1 &&
          numberField(status, "invalidatedFiles") == 1 &&
          numberField(status, "semanticallyAnalyzedFiles") == 1,
      "239-file warm edit confines work to one source: " + status, failures);
#ifdef NDEBUG
  rocket::test::expect(
      numberField(status, "elapsedMilliseconds") < 1000,
      "239-file warm edit completes in under one second: " + status, failures);
#endif
  const auto incrementalDiagnostics = incremental.recorded;
  const auto incrementalSymbols = request(
      incremental, "large-symbols", "workspace/symbol", R"({"query":""})");
  const auto incrementalDocument = request(
      incremental, "large-document", "textDocument/documentSymbol",
      R"({"textDocument":{"uri":)" + jsonQuoted(uri("library")) + R"(}})");
  finish(incremental);

  rocket::test::LspSession clean;
  initialize(clean);
  openAll(clean, true);
  const auto cleanDiagnostics = clean.recorded;
  const auto cleanSymbols =
      request(clean, "large-symbols", "workspace/symbol", R"({"query":""})");
  const auto cleanDocument = request(
      clean, "large-document", "textDocument/documentSymbol",
      R"({"textDocument":{"uri":)" + jsonQuoted(uri("library")) + R"(}})");
  rocket::test::expect(incrementalSymbols == cleanSymbols &&
                           incrementalDocument == cleanDocument,
                       "239-file symbols equal clean full", failures);
  for (const auto &file : files) {
    const auto documentUri = uri(file.name);
    rocket::test::expect(
        latestDiagnostics(incrementalDiagnostics, documentUri) ==
            latestDiagnostics(cleanDiagnostics, documentUri),
        "239-file diagnostics equal clean full for " + file.name, failures);
  }
  finish(clean);
}
void compareUnopenedDependency(int &failures) {
  const auto fixture =
      std::filesystem::current_path() /
      ("lsp-unopened-dependency-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(fixture);
  const auto library = fixture / "library.rocket";
  const auto consumer = fixture / "consumer.rocket";
  const std::string before = "pub fn value() -> Int:\n    return 1\n";
  const std::string after = "pub fn value() -> Int:\n    return 2\n";
  const std::string use =
      "import library\nfn use() -> Int:\n    return library.value()\n";
  {
    std::ofstream stream(library);
    stream << before;
  }
  {
    std::ofstream stream(consumer);
    stream << use;
  }
  const auto rootUri = "file:///" + fixture.generic_string();
  const auto libraryUri = rootUri + "/library.rocket";
  const auto consumerUri = rootUri + "/consumer.rocket";
  const auto begin = [&](rocket::test::LspSession &session) {
    session.send(
        R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{"rootUri":)" +
        jsonQuoted(rootUri) + R"(}})");
    session.response("init");
    session.send(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    session.settle();
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
        jsonQuoted(consumerUri) + R"(,"version":1,"text":)" + jsonQuoted(use) +
        R"(}}})");
    session.settle();
  };

  rocket::test::LspSession incremental;
  begin(incremental);
  {
    std::ofstream stream(library);
    stream << after;
  }
  incremental.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":)" +
      jsonQuoted(libraryUri) + R"(,"type":2}]}})");
  incremental.settle();
  const auto status =
      request(incremental, "unopened-work", "rocket/projectStatus", "{}");
  std::cout << "BENCH unopened dependency warm " << status << '\n';
  rocket::test::expect(
      numberField(status, "reparsedFiles") == 1 &&
          numberField(status, "semanticallyAnalyzedFiles") == 1,
      "unopened private dependency avoids consumer analysis: " + status,
      failures);
  const auto incrementalSymbols = request(
      incremental, "unopened-symbols", "workspace/symbol", R"({"query":""})");
  const auto incrementalDiagnostics = incremental.recorded;
  finish(incremental);

  rocket::test::LspSession clean;
  begin(clean);
  const auto cleanSymbols =
      request(clean, "unopened-symbols", "workspace/symbol", R"({"query":""})");
  const auto cleanDiagnostics = clean.recorded;
  rocket::test::expect(
      incrementalSymbols == cleanSymbols &&
          latestDiagnostics(incrementalDiagnostics, libraryUri) ==
              latestDiagnostics(cleanDiagnostics, libraryUri) &&
          latestDiagnostics(incrementalDiagnostics, consumerUri) ==
              latestDiagnostics(cleanDiagnostics, consumerUri),
      "unopened dependency diagnostics and symbols equal clean full", failures);
  finish(clean);
  std::filesystem::remove_all(fixture);
}
void compareRootlessWatchedDependency(int &failures) {
  const auto fixture =
      std::filesystem::current_path() /
      ("lsp-rootless-watched-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(fixture);
  const auto library = fixture / "library.rocket";
  const auto consumer = fixture / "consumer.rocket";
  const std::string before = "pub fn value() -> Int:\n    return 1\n";
  const std::string after = "pub fn value(x: Int) -> Int:\n    return x\n";
  const std::string use =
      "import library\nfn use() -> Int:\n    return library.value()\n";
  {
    std::ofstream stream(library);
    stream << before;
  }
  const auto libraryUri = fileUri(library);
  const auto consumerUri = fileUri(consumer);
  const auto begin = [&](rocket::test::LspSession &session) {
    initialize(session);
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
        jsonQuoted(consumerUri) + R"(,"version":1,"text":)" + jsonQuoted(use) +
        R"(}}})");
    session.settle();
  };
  rocket::test::LspSession incremental;
  begin(incremental);
  {
    std::ofstream stream(library);
    stream << after;
  }
  incremental.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":)" +
      jsonQuoted(libraryUri) + R"(,"type":2}]}})");
  incremental.settle();
  const auto incrementalDiagnostics = incremental.recorded;
  const auto incrementalSymbols = request(
      incremental, "rootless-symbols", "workspace/symbol", R"({"query":""})");
  finish(incremental);
  rocket::test::LspSession clean;
  begin(clean);
  const auto cleanDiagnostics = clean.recorded;
  const auto cleanSymbols =
      request(clean, "rootless-symbols", "workspace/symbol", R"({"query":""})");
  const auto cleanPublished = latestDiagnostics(cleanDiagnostics, consumerUri);
  rocket::test::expect(
      incrementalSymbols == cleanSymbols &&
          latestDiagnostics(incrementalDiagnostics, consumerUri) ==
              cleanPublished &&
          cleanPublished.find("\"message\":") != std::string::npos,
      "rootless watched imported file matches clean full analysis", failures);
  finish(clean);
  std::filesystem::remove_all(fixture);
}
void compareLongSessionCache(int &failures) {
  rocket::test::LspSession session;
  initialize(session);
  for (int index = 0; index < 120; ++index) {
    const auto name = "transient" + std::to_string(index);
    open(session, name, "fn value() -> Int:\n    return 1\n", 1);
    change(session, name, "fn value() -> Int:\n    return 2\n");
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":)" +
        jsonQuoted(uri(name)) + R"(}}})");
    session.settle();
  }
  const auto fixture =
      std::filesystem::current_path() /
      ("lsp-rootless-imports-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(fixture);
  for (int index = 0; index < 8; ++index) {
    const auto name = "dependency" + std::to_string(index);
    const auto library = fixture / (name + ".rocket");
    const auto consumer =
        fixture / ("consumer" + std::to_string(index) + ".rocket");
    {
      std::ofstream stream(library);
      stream << "pub fn value() -> Int:\n    return 1\n";
    }
    const auto consumerUri = "file:///" + consumer.generic_string();
    const auto source = "import " + name + "\nfn use() -> Int:\n    return " +
                        name + ".value()\n";
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
        jsonQuoted(consumerUri) + R"(,"version":1,"text":)" +
        jsonQuoted(source) + R"(}}})");
    session.settle();
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":)" +
        jsonQuoted(consumerUri) + R"(}}})");
    session.settle();
  }
  const auto status =
      request(session, "cache-size", "rocket/projectStatus", "{}");
  rocket::test::expect(numberField(status, "parsedCacheEntries") == 0,
                       "closed rootless sources and their imports release "
                       "parsed cache owners: " +
                           status,
                       failures);
  const auto symbols =
      request(session, "after-closes", "workspace/symbol", R"({"query":""})");
  rocket::test::LspSession clean;
  initialize(clean);
  const auto cleanSymbols =
      request(clean, "after-closes", "workspace/symbol", R"({"query":""})");
  rocket::test::expect(
      symbols == cleanSymbols,
      "long-session final symbols equal a clean empty workspace", failures);
  finish(clean);
  finish(session);
  std::filesystem::remove_all(fixture);
}
void compareManifestEdit(int &failures) {
  const auto fixture =
      std::filesystem::current_path() /
      ("lsp-incremental-manifest-" +
       std::to_string(
           std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(fixture);
  {
    std::ofstream first(fixture / "first.rocket");
    first << "fn first() -> Int:\n    return 1\n";
    std::ofstream second(fixture / "second.rocket");
    second << "fn second() -> Int:\n    return missing\n";
  }
  const auto writeManifest = [&](const std::string &entry) {
    std::ofstream manifest(fixture / "rocket.toml");
    manifest << "[package]\nname = \"sample\"\nversion = \"0.1.0\"\n"
                "entry = \""
             << entry << "\"\n";
  };
  writeManifest("first.rocket");
  auto rootUri = std::string("file:///") + fixture.generic_string();
#ifndef _WIN32
  rootUri = "file://" + fixture.generic_string();
#endif
  const auto begin = [&](rocket::test::LspSession &session) {
    session.send(
        R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{"rootUri":)" +
        jsonQuoted(rootUri) + R"(}})");
    session.response("init");
    session.send(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
    session.settle();
  };
  rocket::test::LspSession incremental;
  begin(incremental);
  const std::string firstText = "fn first() -> Int:\n    return 1\n";
  const std::string secondText = "fn second() -> Int:\n    return missing\n";
  const auto openWorkspaceFile = [&](rocket::test::LspSession &session,
                                     const std::string &name,
                                     const std::string &text) {
    session.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":)" +
        jsonQuoted(rootUri + "/" + name) + R"(,"version":1,"text":)" +
        jsonQuoted(text) + R"(}}})");
    session.settle();
  };
  openWorkspaceFile(incremental, "first.rocket", firstText);
  openWorkspaceFile(incremental, "second.rocket", secondText);
  writeManifest("second.rocket");
  incremental.send(
      R"({"jsonrpc":"2.0","method":"workspace/didChangeWatchedFiles","params":{"changes":[{"uri":)" +
      jsonQuoted(rootUri + "/rocket.toml") + R"(,"type":2}]}})");
  incremental.settle();
  const auto updatedDiagnostics = incremental.recorded;
  const auto updatedSymbols =
      request(incremental, "symbols", "workspace/symbol", R"({"query":""})");
  const auto updatedStatus =
      request(incremental, "status", "rocket/projectStatus", "{}");
  rocket::test::expect(
      numberField(updatedStatus, "generation") >= 3 &&
          numberField(updatedStatus, "semanticallyAnalyzedFiles") >= 1,
      "manifest edit enters a full dependency-root analysis", failures);
  finish(incremental);
  rocket::test::LspSession clean;
  begin(clean);
  openWorkspaceFile(clean, "first.rocket", firstText);
  openWorkspaceFile(clean, "second.rocket", secondText);
  const auto cleanDiagnostics = clean.recorded;
  const auto cleanSymbols =
      request(clean, "symbols", "workspace/symbol", R"({"query":""})");
  rocket::test::expect(updatedSymbols == cleanSymbols,
                       "manifest edit symbols equal clean full", failures);
  for (const auto &name : {"first.rocket", "second.rocket"}) {
    const auto documentUri = rootUri + "/" + name;
    const auto updated = latestDiagnostics(updatedDiagnostics, documentUri);
    const auto full = latestDiagnostics(cleanDiagnostics, documentUri);
    rocket::test::expect(
        !updated.empty() && updated == full,
        std::string("manifest edit diagnostics equal clean full for ") + name,
        failures);
  }
  finish(clean);
  std::filesystem::remove_all(fixture);
}
void compareBurst(int &failures) {
  rocket::test::LspSession incremental;
  initialize(incremental);
  open(incremental, "burst", "fn value() -> Int:\n    return 1\n", 1);
  for (int version = 2; version <= 31; ++version)
    incremental.send(
        R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":)" +
        jsonQuoted(uri("burst")) + R"(,"version":)" + std::to_string(version) +
        R"(},"contentChanges":[{"text":)" +
        jsonQuoted("fn value() -> Int:\n    return " + std::to_string(version) +
                   "\n") +
        R"(}]}})");
  incremental.settle();
  const auto status =
      request(incremental, "status", "rocket/projectStatus", "{}");
  rocket::test::expect(numberField(status, "generation") == 31 &&
                           numberField(status, "requestedGeneration") == 31,
                       "rapid burst publishes only the latest generation",
                       failures);
  const auto incrementalDiagnostics =
      latestDiagnostics(incremental.recorded, uri("burst"));
  const auto incrementalSymbols =
      request(incremental, "symbols", "workspace/symbol", R"({"query":""})");
  finish(incremental);
  rocket::test::LspSession clean;
  initialize(clean);
  open(clean, "burst", "fn value() -> Int:\n    return 31\n", 31);
  const auto cleanDiagnostics = latestDiagnostics(clean.recorded, uri("burst"));
  const auto cleanSymbols =
      request(clean, "symbols", "workspace/symbol", R"({"query":""})");
  rocket::test::expect(!incrementalDiagnostics.empty() &&
                           incrementalDiagnostics == cleanDiagnostics &&
                           incrementalSymbols == cleanSymbols,
                       "burst final diagnostics and symbols equal clean full",
                       failures);
  finish(clean);
}
} // namespace

int runTests() {
  int failures = 0;
  rocket::test::LspSession session;
  session.send(
      R"({"jsonrpc":"2.0","id":"init","method":"initialize","params":{}})");
  const auto capabilities = session.response("init");
  rocket::test::expect(
      capabilities.find("\"documentSymbolProvider\":true") !=
              std::string::npos &&
          capabilities.find("\"foldingRangeProvider\":true") !=
              std::string::npos,
      "server advertises authoritative document structure methods", failures);
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/alpha.rocket","version":1,"text":"pub fn alpha() -> Int:\n    return 1\n"}}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/beta.rocket","version":1,"text":"pub fn beta() -> Int:\n    return 2\n"}}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/alpha.rocket","version":2},"contentChanges":[{"text":"pub fn alpha() -> Int:\n    return 3\n"}]}})");
  session.settle();
  session.send(
      R"({"jsonrpc":"2.0","id":"status","method":"rocket/projectStatus"})");
  const auto status = session.response("status");
  rocket::test::expect(
      status.find("\"reparsedFiles\":1") != std::string::npos &&
          status.find("\"semanticallyAnalyzedFiles\":1") != std::string::npos,
      "one-module edit does not reparse or analyze unrelated roots: " + status,
      failures);
  session.send(
      R"({"jsonrpc":"2.0","id":"symbols","method":"textDocument/documentSymbol","params":{"textDocument":{"uri":"file:///C:/workspace/alpha.rocket"}}})");
  const auto symbols = session.response("symbols");
  rocket::test::expect(
      symbols.find("\"name\":\"alpha\"") != std::string::npos &&
          symbols.find("\"kind\":12") != std::string::npos &&
          symbols.find("\"selectionRange\"") != std::string::npos,
      "document symbols use parsed declarations with LSP symbol kinds",
      failures);
  session.send(
      R"({"jsonrpc":"2.0","id":"folds","method":"textDocument/foldingRange","params":{"textDocument":{"uri":"file:///C:/workspace/alpha.rocket"}}})");
  const auto folds = session.response("folds");
  rocket::test::expect(
      folds.find("\"startLine\":0") != std::string::npos &&
          folds.find("\"endLine\":1") != std::string::npos,
      "folding range follows parser indentation for function body", failures);
  session.send(
      R"({"jsonrpc":"2.0","id":"workspaceSymbols","method":"workspace/symbol","params":{"query":"alpha"}})");
  const auto workspaceSymbols = session.response("workspaceSymbols");
  rocket::test::expect(
      workspaceSymbols.find("\"kind\":12") != std::string::npos,
      "workspace symbols use LSP SymbolKind rather than completion kinds",
      failures);
  session.send(R"({"jsonrpc":"2.0","id":"shutdown","method":"shutdown"})");
  session.response("shutdown");
  session.send(R"({"jsonrpc":"2.0","method":"exit"})");
  session.finish();
  rocket::test::LspSession structure;
  initialize(structure);
  open(structure, "shapes",
       "pub struct Pair:\n    first: Int\n    second: Int\n"
       "pub enum Signal:\n    Value(Int)\n    Empty\n"
       "pub fn doubled(value: Int) -> Int:\n    return value * 2\n",
       1);
  const auto structured = request(
      structure, "file", "textDocument/documentSymbol",
      R"({"textDocument":{"uri":"file:///C:/workspace/shapes.rocket"}})");
  rocket::test::expect(
      structured.find("\"name\":\"Pair\"") != std::string::npos &&
          structured.find("\"name\":\"first\"") != std::string::npos &&
          structured.find("\"name\":\"Signal\"") != std::string::npos &&
          structured.find("\"name\":\"Value\"") != std::string::npos &&
          structured.find("\"name\":\"doubled\"") != std::string::npos,
      "document symbols include parsed types, fields, variants, and functions",
      failures);
  const auto enumWorkspace =
      request(structure, "enum", "workspace/symbol", R"({"query":"Signal"})");
  rocket::test::expect(enumWorkspace.find("\"kind\":10") != std::string::npos,
                       "workspace enum uses LSP Enum SymbolKind", failures);
  const auto structureFolds = request(
      structure, "fold", "textDocument/foldingRange",
      R"({"textDocument":{"uri":"file:///C:/workspace/shapes.rocket"}})");
  rocket::test::expect(
      structureFolds.find("\"startLine\":0") != std::string::npos &&
          structureFolds.find("\"startLine\":3") != std::string::npos &&
          structureFolds.find("\"startLine\":6") != std::string::npos,
      "folding spans follow parser blocks for struct, enum, and function",
      failures);
  const auto structureStatus =
      request(structure, "status", "rocket/projectStatus", "{}");
  rocket::test::expect(
      structureStatus.find("\"generation\":1") != std::string::npos &&
          structureStatus.find("\"files\":1") != std::string::npos &&
          numberField(structureStatus, "symbols") > 0,
      "project status reports the published compiler snapshot", failures);
  finish(structure);
  compareCleanFull("one file",
                   {{"single", "fn value() -> Int:\n    return 1\n",
                     "fn value() -> Int:\n    return 2\n"}},
                   0, failures);
  compareCleanFull(
      "ordinary package",
      {{"math", "pub fn doubled(value: Int) -> Int:\n    return value * 2\n",
        "pub fn doubled(value: Int) -> Int:\n    return value * 3\n"},
       {"main",
        "import math\nfn result() -> Int:\n    return math.doubled(2)\n",
        "import math\nfn result() -> Int:\n    return math.doubled(2)\n"}},
      0, failures);
  compareCleanFull("unrelated modules",
                   {{"a", "fn a() -> Int:\n    return 1\n",
                     "fn a() -> Int:\n    return 2\n"},
                    {"b", "fn b() -> Int:\n    return 3\n",
                     "fn b() -> Int:\n    return 3\n"},
                    {"c", "fn c() -> Int:\n    return 4\n",
                     "fn c() -> Int:\n    return 4\n"}},
                   0, failures);
  compareCleanFull(
      "deep chain",
      {{"leaf", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 2\n"},
       {"middle",
        "import leaf\npub fn middle() -> Int:\n    return leaf.value()\n",
        "import leaf\npub fn middle() -> Int:\n    return leaf.value()\n"},
       {"top", "import middle\nfn top() -> Int:\n    return middle.middle()\n",
        "import middle\nfn top() -> Int:\n    return middle.middle()\n"}},
      0, failures);
  compareCleanFull(
      "fan in and out",
      {{"core", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 2\n"},
       {"left", "import core\nfn left() -> Int:\n    return core.value()\n",
        "import core\nfn left() -> Int:\n    return core.value()\n"},
       {"right", "import core\nfn right() -> Int:\n    return core.value()\n",
        "import core\nfn right() -> Int:\n    return core.value()\n"},
       {"other", "fn other() -> Int:\n    return 5\n",
        "fn other() -> Int:\n    return 5\n"}},
      0, failures);
  compareCleanFull(
      "public interface",
      {{"library", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Bool:\n    return true\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 3);
  compareCleanFull(
      "private body",
      {{"library",
        "pub fn value() -> Int:\n    return hidden()\nfn hidden() -> Int:\n    "
        "return 1\n",
        "pub fn value() -> Int:\n    return hidden()\nfn hidden() -> Int:\n    "
        "return 2\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 1);
  compareCleanFull(
      "public body stable signature",
      {{"library", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 2\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 1);
  compareCleanFull(
      "public generic body",
      {{"library", "pub fn identity[T](value: T) -> T:\n    return value\n",
        "pub fn identity[T](value: T) -> T:\n    # body edit\n    return "
        "value\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.identity(1)\n",
        "import library\nfn use() -> Int:\n    return library.identity(1)\n"}},
      0, failures, 3);
  compareCleanFull(
      "private body adds local",
      {{"library",
        "pub fn value() -> Int:\n    return hidden()\n"
        "fn hidden() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return hidden()\n"
        "fn hidden() -> Int:\n    let local = 2\n    return local\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 2);
  compareCleanFull(
      "reference order",
      {{"dependency", "pub fn value() -> Int:\n    return value()\n",
        "pub fn value() -> Int:\n    return value()\n"},
       {"library",
        "import dependency\npub fn call() -> Int:\n    return "
        "dependency.value()\n",
        "import dependency\npub fn call() -> Int:\n    let local = "
        "dependency.value()\n    return local\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.call()\n",
        "import library\nfn use() -> Int:\n    return library.call()\n"}},
      1, failures, 3);
  compareCleanFull(
      "public signature change",
      {{"library", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value(x: Int) -> Int:\n    return x\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 3);
  compareCleanFull(
      "export added",
      {{"library", "fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 1\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 3);
  compareCleanFull(
      "export removed",
      {{"library", "pub fn value() -> Int:\n    return 1\n",
        "fn value() -> Int:\n    return 1\n"},
       {"consumer",
        "import library\nfn use() -> Int:\n    return library.value()\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      0, failures, 1);
  compareCleanFull(
      "exported struct shape",
      {{"library", "pub struct Pair:\n    value: Int\n",
        "pub struct Pair:\n    value: Bool\n"},
       {"consumer", "import library\nfn use() -> Int:\n    return 1\n",
        "import library\nfn use() -> Int:\n    return 1\n"}},
      0, failures, 3);
  compareCleanFull(
      "exported enum shape",
      {{"library", "pub enum Signal:\n    Value(Int)\n    Empty\n",
        "pub enum Signal:\n    Value(Bool)\n    Empty\n"},
       {"consumer", "import library\nfn use() -> Int:\n    return 1\n",
        "import library\nfn use() -> Int:\n    return 1\n"}},
      0, failures, 3);
  compareCleanFull(
      "four-module private chain",
      {{"d", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 2\n"},
       {"c", "import d\npub fn value() -> Int:\n    return d.value()\n",
        "import d\npub fn value() -> Int:\n    return d.value()\n"},
       {"b", "import c\npub fn value() -> Int:\n    return c.value()\n",
        "import c\npub fn value() -> Int:\n    return c.value()\n"},
       {"a", "import b\nfn value() -> Int:\n    return b.value()\n",
        "import b\nfn value() -> Int:\n    return b.value()\n"}},
      0, failures, 1);
  compareCleanFull(
      "four-module local chain",
      {{"d", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    let local = 2\n    return local\n"},
       {"c", "import d\npub fn value() -> Int:\n    return d.value()\n",
        "import d\npub fn value() -> Int:\n    return d.value()\n"},
       {"b", "import c\npub fn value() -> Int:\n    return c.value()\n",
        "import c\npub fn value() -> Int:\n    return c.value()\n"},
       {"a", "import b\nfn value() -> Int:\n    return b.value()\n",
        "import b\nfn value() -> Int:\n    return b.value()\n"}},
      0, failures, 2);
  compareCleanFull(
      "four-module public chain",
      {{"d", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Bool:\n    return true\n"},
       {"c", "import d\npub fn value() -> Int:\n    return d.value()\n",
        "import d\npub fn value() -> Int:\n    return d.value()\n"},
       {"b", "import c\npub fn value() -> Int:\n    return c.value()\n",
        "import c\npub fn value() -> Int:\n    return c.value()\n"},
       {"a", "import b\nfn value() -> Int:\n    return b.value()\n",
        "import b\nfn value() -> Int:\n    return b.value()\n"}},
      0, failures, 10);
  compareCleanFull("fan in private branch",
                   {{"left", "pub fn value() -> Int:\n    return 1\n",
                     "pub fn value() -> Int:\n    return 2\n"},
                    {"middle", "pub fn value() -> Int:\n    return 3\n",
                     "pub fn value() -> Int:\n    return 3\n"},
                    {"right", "pub fn value() -> Int:\n    return 4\n",
                     "pub fn value() -> Int:\n    return 4\n"},
                    {"consumer",
                     "import left\nimport middle\nimport right\n"
                     "fn value() -> Int:\n    return left.value() + "
                     "middle.value() + right.value()\n",
                     "import left\nimport middle\nimport right\n"
                     "fn value() -> Int:\n    return left.value() + "
                     "middle.value() + right.value()\n"}},
                   0, failures, 1);
  std::vector<FixtureFile> fanOut{{"library",
                                   "pub fn value() -> Int:\n    return 1\n",
                                   "pub fn value() -> Int:\n    return 2\n"}};
  for (int index = 0; index < 8; ++index) {
    const auto source =
        "import library\nfn use() -> Int:\n    return library.value()\n";
    fanOut.push_back({"consumer" + std::to_string(index), source, source});
  }
  compareCleanFull("fan out private body", fanOut, 0, failures, 1);
  fanOut.front().after =
      "pub fn value() -> Int:\n    let local = 2\n    return local\n";
  compareCleanFull("fan out local body", fanOut, 0, failures, 2);
  fanOut.front().after = "pub fn value() -> Bool:\n    return true\n";
  compareCleanFull("fan out public interface", fanOut, 0, failures, 17);
  compareCleanFull(
      "import edit",
      {{"library", "pub fn value() -> Int:\n    return 1\n",
        "pub fn value() -> Int:\n    return 1\n"},
       {"consumer", "fn use() -> Int:\n    return 1\n",
        "import library\nfn use() -> Int:\n    return library.value()\n"}},
      1, failures);
  compareManifestEdit(failures);
  compareBurst(failures);
  compareCleanFull("bare Unit return",
                   {{"unit", "fn action() -> Unit:\n    return\n",
                     "fn action() -> Unit:\n    let value = 1\n    return\n"}},
                   0, failures);
  std::vector<FixtureFile> many;
  for (int index = 0; index < 48; ++index) {
    const auto name = "module" + std::to_string(index);
    many.push_back({name, "fn value() -> Int:\n    return 1\n",
                    index == 0 ? "fn value() -> Int:\n    return 2\n"
                               : "fn value() -> Int:\n    return 1\n"});
  }
  compareCleanFull("many unrelated modules", many, 0, failures);
  compareLargeWorkspace(failures);
  compareUnopenedDependency(failures);
  compareRootlessWatchedDependency(failures);
  compareLongSessionCache(failures);
  // The production-sized source is a Release probe; slower Debug workers can
  // exceed its 180-second settle bound without a semantic failure.
#if defined(NDEBUG) && !defined(ROCKETC_ASAN_TEST)
  const auto largePath =
      std::filesystem::path(__FILE__).parent_path().parent_path() /
      "compiler/src/main.rocket";
  std::ifstream largeInput(largePath, std::ios::binary);
  std::ostringstream largeBuffer;
  largeBuffer << largeInput.rdbuf();
  rocket::test::expect(largeInput.good() || largeInput.eof(),
                       "reference large Rocket source is readable", failures);
  if (largeBuffer.str().size() > 500000)
    compareCleanFull("large unrelated source",
                     {{"large", largeBuffer.str(), largeBuffer.str()},
                      {"leaf", "fn leaf() -> Int:\n    return 1\n",
                       "fn leaf() -> Int:\n    return 2\n"}},
                     1, failures);
#endif
  return failures == 0 ? 0 : 1;
}

int main() {
  try {
    return runTests();
  } catch (const std::exception &error) {
    std::cerr << "UNCAUGHT " << error.what() << std::endl;
    return 1;
  }
}
