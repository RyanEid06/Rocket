#include "language_server.h"
#include "test_support.h"

#include <charconv>
#include <sstream>
#include <string>
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
  rocket::test::expect(validFrames && messages.size() == 6,
                       "language server emits six correctly framed responses", failures);
  const std::string responses = responseStream.str();
  rocket::test::expect(
      responses.find("\"positionEncoding\":\"utf-16\"") != std::string::npos &&
          responses.find("\"textDocumentSync\"") != std::string::npos,
      "initialize advertises UTF-16 full-document synchronization", failures);
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

  return rocket::test::finish(failures, "language server");
}
