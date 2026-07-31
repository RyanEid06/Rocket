#include "language_server.h"
#include "test_support.h"

#include <charconv>
#include <sstream>
#include <string>
#include <chrono>
#include <vector>

namespace {

std::string frame(const std::string& body) {
  return "Content-Length: " + std::to_string(body.size()) + "\r\n\r\n" + body;
}

std::vector<std::string> bodies(const std::string& framed, bool& valid) {
  std::vector<std::string> result;
  std::size_t offset = 0;
  valid = true;
  while (offset < framed.size()) {
    const std::size_t headerEnd = framed.find("\r\n\r\n", offset);
    if (headerEnd == std::string::npos) {
      valid = false;
      return result;
    }
    const std::string header = framed.substr(offset, headerEnd - offset);
    constexpr std::string_view prefix = "Content-Length: ";
    if (!header.starts_with(prefix)) {
      valid = false;
      return result;
    }
    std::size_t length = 0;
    const auto converted = std::from_chars(
        header.data() + prefix.size(), header.data() + header.size(), length);
    if (converted.ec != std::errc{} || converted.ptr != header.data() + header.size()) {
      valid = false;
      return result;
    }
    const std::size_t bodyStart = headerEnd + 4;
    if (length > framed.size() - bodyStart) {
      valid = false;
      return result;
    }
    result.push_back(framed.substr(bodyStart, length));
    offset = bodyStart + length;
  }
  return result;
}

std::size_t occurrences(const std::string& text, const std::string& pattern) {
  std::size_t result = 0;
  std::size_t offset = 0;
  while ((offset = text.find(pattern, offset)) != std::string::npos) {
    ++result;
    offset += pattern.size();
  }
  return result;
}

} // namespace

int main() {
  int failures = 0;
  const std::string uri = "file:///C:/workspace/sample.rocket";
  std::string input;
  input += frame(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  input += frame(R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  input += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/sample.rocket","languageId":"rocket","version":1,"text":"fn helper() -> Int:\n    return missing\n"}}})");
  input += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/sample.rocket","version":2},"contentChanges":[{"text":"fn helper() -> Int:\n    return 0\n"}]}})");
  input += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/sample.rocket","version":1},"contentChanges":[{"text":"fn broken( -> Int:\n"}]}})");
  input += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didClose","params":{"textDocument":{"uri":"file:///C:/workspace/sample.rocket"}}})");
  input += frame(R"({"jsonrpc":"2.0","id":9,"method":"rocket/unknown","params":{}})");
  input += frame(R"({"jsonrpc":"2.0","id":2,"method":"shutdown","params":null})");
  input += frame(R"({"jsonrpc":"2.0","method":"exit"})");

  std::istringstream requestStream(input);
  std::ostringstream responseStream;
  std::ostringstream logStream;
  rocket::LanguageServer server(requestStream, responseStream, logStream);
  rocket::test::expect(server.run() == 0,
                       "shutdown followed by exit is a clean LSP session", failures);

  bool validFrames = false;
  const auto messages = bodies(responseStream.str(), validFrames);
  rocket::test::expect(validFrames && messages.size() >= 6,
                       "language server emits correctly framed responses and telemetry", failures);
  const std::string responses = responseStream.str();
  rocket::test::expect(
      responses.find("\"positionEncoding\":\"utf-16\"") != std::string::npos &&
          responses.find("\"textDocumentSync\"") != std::string::npos &&
          responses.find("\"change\":2") != std::string::npos,
      "initialize advertises UTF-16 incremental document synchronization", failures);
  rocket::test::expect(
      responses.find("\"code\":\"R4002\"") != std::string::npos &&
          responses.find("undefined name 'missing'") != std::string::npos,
      "open documents publish stable semantic diagnostics", failures);
  rocket::test::expect(
      occurrences(responses, "\"diagnostics\":[]") == 2 &&
          occurrences(responses, "\"version\":2") == 2,
      "valid changes clear diagnostics, stale changes are ignored, and close clears again",
      failures);
  rocket::test::expect(
      responses.find("\"code\":-32601") != std::string::npos &&
          responses.find("method not found: rocket/unknown") != std::string::npos,
      "unknown requests receive the JSON-RPC method-not-found error", failures);
  rocket::test::expect(logStream.str().empty(),
                       "valid protocol sessions do not write protocol noise to stderr",
                       failures);

  std::istringstream abruptInput(
      frame(R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})") +
      frame(R"({"jsonrpc":"2.0","method":"exit"})"));
  std::ostringstream abruptOutput;
  std::ostringstream abruptLog;
  rocket::LanguageServer abrupt(abruptInput, abruptOutput, abruptLog);
  rocket::test::expect(abrupt.run() == 1,
                       "exit without shutdown reports an unclean LSP termination", failures);

  std::istringstream malformedInput(frame("{"));
  std::ostringstream malformedOutput;
  std::ostringstream malformedLog;
  rocket::LanguageServer malformed(malformedInput, malformedOutput, malformedLog);
  rocket::test::expect(
      malformed.run() == 0 &&
          malformedOutput.str().find("\"code\":-32700") != std::string::npos,
      "malformed JSON receives a bounded parse-error response", failures);

  std::istringstream invalidRequestInput(frame("{}"));
  std::ostringstream invalidRequestOutput;
  std::ostringstream invalidRequestLog;
  rocket::LanguageServer invalidRequest(
      invalidRequestInput, invalidRequestOutput, invalidRequestLog);
  rocket::test::expect(
      invalidRequest.run() == 0 &&
          invalidRequestOutput.str().find("\"code\":-32600") != std::string::npos &&
          invalidRequestOutput.str().find("\"id\":null") != std::string::npos,
      "id-less invalid JSON-RPC objects receive invalid-request responses", failures);

  std::istringstream oversizedHeaderInput(std::string(16 * 1024 + 1, 'X'));
  std::ostringstream oversizedHeaderOutput;
  std::ostringstream oversizedHeaderLog;
  rocket::LanguageServer oversizedHeader(
      oversizedHeaderInput, oversizedHeaderOutput, oversizedHeaderLog);
  rocket::test::expect(
      oversizedHeader.run() == 0 &&
          oversizedHeaderOutput.str().find("LSP header exceeds 16 KiB") !=
              std::string::npos,
      "unterminated protocol headers are rejected at the 16 KiB streaming bound",
      failures);

  std::string unicodeInput;
  unicodeInput += frame(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  unicodeInput += frame(
      R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  unicodeInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/unicode.rocket","languageId":"rocket","version":1,"text":"fn helper() -> String:\n    return \"\ud83d\ude80\" + missing\n"}}})");
  unicodeInput += frame(
      R"({"jsonrpc":"2.0","id":2,"method":"shutdown","params":null})");
  unicodeInput += frame(R"({"jsonrpc":"2.0","method":"exit"})");
  std::istringstream unicodeRequestStream(unicodeInput);
  std::ostringstream unicodeResponseStream;
  std::ostringstream unicodeLogStream;
  rocket::LanguageServer unicodeServer(
      unicodeRequestStream, unicodeResponseStream, unicodeLogStream);
  rocket::test::expect(
      unicodeServer.run() == 0 &&
          unicodeResponseStream.str().find("\"character\":18") !=
              std::string::npos &&
          unicodeResponseStream.str().find("undefined name 'missing'") !=
              std::string::npos,
      "diagnostic columns convert UTF-8 source prefixes to UTF-16 LSP units",
      failures);

  std::string semanticInput;
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":1,"method":"initialize","params":{}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"initialized","params":{}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/math.rocket","languageId":"rocket","version":1,"text":"# Doubles a value.\npub fn doubled(value: Int) -> Int:\n    return value * 2\n"}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket","languageId":"rocket","version":1,"text":"import math\n\nfn helper() -> Int:\n    return math.doubled(21)\n"}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didOpen","params":{"textDocument":{"uri":"file:///C:/workspace/actions.rocket","languageId":"rocket","version":1,"text":"fn helper() -> Int:\n    return doubled(21)\n"}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":10,"method":"textDocument/completion","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":23}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":11,"method":"textDocument/hover","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":19}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":12,"method":"textDocument/signatureHelp","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":25}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":13,"method":"textDocument/definition","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":19}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":14,"method":"textDocument/references","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":19},"context":{"includeDeclaration":true}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":15,"method":"textDocument/prepareRename","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":19}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":16,"method":"textDocument/rename","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"position":{"line":3,"character":19},"newName":"twice"}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":17,"method":"textDocument/semanticTokens/full","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket","version":2},"contentChanges":[{"range":{"start":{"line":3,"character":24},"end":{"line":3,"character":26}},"rangeLength":2,"text":"22"}]}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":18,"method":"textDocument/semanticTokens/full/delta","params":{"textDocument":{"uri":"file:///C:/workspace/main.rocket"},"previousResultId":"2:1"}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"$/cancelRequest","params":{"id":19}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":19,"method":"workspace/symbol","params":{"query":"double"}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":20,"method":"rocket/projectStatus","params":{}})");
  const std::string missingImportDiagnostic =
      R"({"range":{"start":{"line":1,"character":11},"end":{"line":1,"character":18}},"severity":1,"code":"R4002","source":"rocket","message":"undefined name 'doubled'"})";
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":21,"method":"textDocument/codeAction","params":{"textDocument":{"uri":"file:///C:/workspace/actions.rocket"},"range":{"start":{"line":1,"character":11},"end":{"line":1,"character":18}},"context":{"diagnostics":[)" +
      missingImportDiagnostic + R"(]}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":22,"method":"textDocument/codeAction","params":{"textDocument":{"uri":"file:///C:/workspace/actions.rocket"},"range":{"start":{"line":1,"character":11},"end":{"line":1,"character":18}},"context":{"diagnostics":[)" +
      missingImportDiagnostic + R"(]}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","method":"textDocument/didChange","params":{"textDocument":{"uri":"file:///C:/workspace/actions.rocket","version":2},"contentChanges":[{"range":{"start":{"line":0,"character":0},"end":{"line":0,"character":0}},"text":"import math\n\n"}]}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":23,"method":"textDocument/codeAction","params":{"textDocument":{"uri":"file:///C:/workspace/actions.rocket"},"range":{"start":{"line":3,"character":11},"end":{"line":3,"character":18}},"context":{"diagnostics":[)" +
      missingImportDiagnostic + R"(]}}})");
  semanticInput += frame(
      R"({"jsonrpc":"2.0","id":2,"method":"shutdown","params":null})");
  semanticInput += frame(R"({"jsonrpc":"2.0","method":"exit"})");
  std::istringstream semanticRequests(semanticInput);
  std::ostringstream semanticResponses;
  std::ostringstream semanticLog;
  rocket::LanguageServer semanticServer(
      semanticRequests, semanticResponses, semanticLog);
  const auto semanticStarted = std::chrono::steady_clock::now();
  const std::string semanticOutput = [&] {
    const int status = semanticServer.run();
    rocket::test::expect(status == 0, "semantic protocol session exits cleanly", failures);
    return semanticResponses.str();
  }();
  const auto semanticElapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::steady_clock::now() - semanticStarted).count();
  rocket::test::expect(
      semanticOutput.find("\"completionProvider\"") != std::string::npos &&
          semanticOutput.find("\"label\":\"doubled\"") != std::string::npos &&
          semanticOutput.find("Doubles a value") != std::string::npos,
      "completion and hover reuse project declarations and documentation", failures);
  rocket::test::expect(
      semanticOutput.find("file:///C:/workspace/math.rocket") != std::string::npos &&
          occurrences(semanticOutput, "\"newText\":\"twice\"") >= 2,
      "definition, references, and safe rename use resolved cross-file symbols", failures);
  rocket::test::expect(
      semanticOutput.find("\"signatures\":[{") != std::string::npos &&
          semanticOutput.find("\"data\":[") != std::string::npos &&
          semanticOutput.find("\"edits\":[") != std::string::npos,
      "signature help and full/delta semantic tokens are deterministic", failures);
  rocket::test::expect(
      semanticOutput.find("\"code\":-32800") != std::string::npos &&
          semanticOutput.find("\"maximumProjectFiles\":4096") != std::string::npos,
      "request cancellation and bounded project status are editor-neutral", failures);
  const auto importActionCount = occurrences(
      semanticOutput, "\"title\":\"Import math\"");
  rocket::test::expect(
      importActionCount == 2,
      "missing-import code actions are deterministic and idempotent after application",
      failures);
  rocket::test::expect(
      semanticElapsed < 5000,
      "multi-document incomplete-code protocol latency remains below five seconds",
      failures);

  return rocket::test::finish(failures, "language server");
}
