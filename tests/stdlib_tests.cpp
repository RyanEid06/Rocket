#include "runtime.h"
#include "platform_net.h"
#include "safe_archive.h"
#include "safe_regex.h"
#include "test_support.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

namespace {

RocketString* string(const std::string& value) {
  return rocket_rt_string_new(reinterpret_cast<const std::uint8_t*>(value.data()), value.size());
}

std::string value(const RocketString* text) {
  return std::string(reinterpret_cast<const char*>(rocket_rt_string_bytes(text)),
                     static_cast<std::size_t>(rocket_rt_string_byte_length(text)));
}

} // namespace

int main(int argc, char** argv) {
  int failures = 0;
  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "standard-library test starts without live objects", failures);

  RocketString* rocket = string("Rocket");
  RocketString* suffix = string(" Library");
  RocketString* joined = rocket_std_string_concat(rocket, suffix);
  rocket::test::expect(value(joined) == "Rocket Library" &&
                           rocket_std_string_byte_length(joined) == 14,
                       "string construction and byte length are deterministic", failures);
  RocketString* needle = string("Library");
  rocket::test::expect(rocket_std_string_contains(joined, needle) == 1 &&
                           rocket_std_string_ends_with(joined, needle) == 1,
                       "string search helpers operate on UTF-8 byte sequences", failures);
  RocketString* delimiter = string(" ");
  RocketArray* pieces = rocket_std_string_split(joined, delimiter);
  RocketArray* reversed = rocket_std_collections_reverse(pieces);
  RocketString* dash = string("-");
  RocketString* reverseJoined = rocket_std_collections_join(reversed, dash);
  rocket::test::expect(value(reverseJoined) == "Library-Rocket",
                       "collection reverse and string join preserve typed elements", failures);
  rocket::test::expect(rocket_std_string_byte_at(joined, 0) == 'R',
                       "string.byte_at exposes deterministic UTF-8 bytes", failures);
  rocket::test::expect(rocket_std_string_byte_value_at(joined, 0) == 82,
                       "string.byte_value_at exposes an unsigned numeric byte", failures);
  RocketString* sliced = rocket_std_string_slice(joined, 0, 6);
  rocket::test::expect(value(sliced) == "Rocket",
                       "string.slice copies an exclusive byte range", failures);
  RocketStringBuilder* builder = rocket_std_string_builder();
  rocket_std_string_builder_append(builder, sliced);
  rocket_std_string_builder_append(builder, suffix);
  RocketString* built = rocket_std_string_builder_finish(builder);
  rocket::test::expect(value(built) == "Rocket Library",
                       "String Builder appends and freezes deterministic text", failures);
  rocket_rt_release(built);
  rocket_rt_release(builder);
  RocketArray* concatenated = rocket_std_collections_concat(pieces, reversed);
  rocket::test::expect(rocket_rt_collection_length(concatenated) == 4,
                       "collections.concat preserves generic managed elements", failures);
  rocket_rt_release(concatenated);
  rocket_rt_release(sliced);
  rocket_rt_release(reverseJoined);
  rocket_rt_release(dash);
  rocket_rt_release(reversed);
  rocket_rt_release(pieces);
  rocket_rt_release(delimiter);
  rocket_rt_release(needle);
  rocket_rt_release(joined);
  rocket_rt_release(suffix);
  rocket_rt_release(rocket);

  RocketString* integerText = string("9223372036854775807");
  RocketAggregate* parsedInteger = rocket_std_string_parse_int(integerText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedInteger) == 0 &&
                           rocket_rt_aggregate_get_int(parsedInteger, 0) == INT64_MAX,
                       "string.parse_int returns Result[Int, String]", failures);
  rocket_rt_release(parsedInteger);
  rocket_rt_release(integerText);

  RocketString* jsonText = string("{\"name\":\"Rocket\",\"values\":[1,2],\"ok\":true}");
  RocketAggregate* parsedJson = rocket_std_json_parse(jsonText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedJson) == 0,
                       "valid JSON returns Ok(Json)", failures);
  auto* json = static_cast<RocketAggregate*>(rocket_rt_aggregate_get_managed(parsedJson, 0));
  RocketString* encodedJson = rocket_std_json_stringify(json);
  rocket::test::expect(value(encodedJson) ==
                           "{\"name\":\"Rocket\",\"values\":[1,2],\"ok\":true}",
                       "JSON parse/stringify round-trips ordered values", failures);
  rocket_rt_release(encodedJson);
  rocket_rt_release(json);
  rocket_rt_release(parsedJson);
  rocket_rt_release(jsonText);
  RocketString* badJsonText = string("{]");
  RocketAggregate* badJson = rocket_std_json_parse(badJsonText);
  rocket::test::expect(rocket_rt_aggregate_tag(badJson) == 1,
                       "invalid JSON returns Err(String)", failures);
  rocket_rt_release(badJson);
  rocket_rt_release(badJsonText);

  RocketString* csvText = string("name,value\nrocket,7");
  RocketAggregate* parsedCsv = rocket_std_csv_parse(csvText);
  rocket::test::expect(rocket_rt_aggregate_tag(parsedCsv) == 0,
                       "valid CSV returns nested string rows", failures);
  auto* rows = static_cast<RocketArray*>(rocket_rt_aggregate_get_managed(parsedCsv, 0));
  RocketString* encodedCsv = rocket_std_csv_encode(rows);
  rocket::test::expect(value(encodedCsv) == "name,value\r\nrocket,7",
                       "CSV encoding uses deterministic RFC-style CRLF rows", failures);
  rocket_rt_release(encodedCsv);
  rocket_rt_release(rows);
  rocket_rt_release(parsedCsv);
  rocket_rt_release(csvText);

  rocket_std_random_seed(12345);
  const std::int64_t firstRandom = rocket_std_random_int(-10, 10);
  const double firstFloat = rocket_std_random_float();
  rocket_std_random_seed(12345);
  rocket::test::expect(firstRandom == rocket_std_random_int(-10, 10) &&
                           firstFloat == rocket_std_random_float() &&
                           firstRandom >= -10 && firstRandom < 10,
                       "seeded random sequences are reproducible and range-bounded", failures);

  const auto alternateMatch = rocket::safe_regex::search("ab|b", "xb");
  const auto longestMatch = rocket::safe_regex::search("a|aa", "aa");
  const auto emptyMatches = rocket::safe_regex::findAll("", "ab");
  std::string nestedPattern(257, '(');
  nestedPattern += "a";
  nestedPattern.append(257, ')');
  const auto nestedResult = rocket::safe_regex::search(nestedPattern, "a");
  const auto longMiss = rocket::safe_regex::search("a+$", std::string(100000, 'b'));
  rocket::test::expect(
      alternateMatch.valid && alternateMatch.matches.size() == 1 &&
          alternateMatch.matches[0].start == 1 && alternateMatch.matches[0].end == 2 &&
          longestMatch.valid && longestMatch.matches.size() == 1 &&
          longestMatch.matches[0].start == 0 && longestMatch.matches[0].end == 2 &&
          emptyMatches.valid && emptyMatches.matches.size() == 3 &&
          !nestedResult.valid && longMiss.valid && longMiss.matches.empty(),
      "safe regex preserves leftmost-longest results, empty matches, nesting bounds, and long misses",
      failures);

  const std::filesystem::path archiveTemporary =
      std::filesystem::current_path() / "rocket_stdlib_test_temp.tar";
  std::string archiveError;
  const bool archiveCreated = rocket::safe_archive::create(
      archiveTemporary.string(), {{"safe/value.txt", "value"}}, archiveError);
  std::vector<std::string> archiveNames;
  const bool archiveListed = archiveCreated && rocket::safe_archive::list(
      archiveTemporary.string(), archiveNames, archiveError);
  std::filesystem::resize_file(archiveTemporary,
                               std::filesystem::file_size(archiveTemporary) - 512);
  std::vector<std::string> truncatedNames;
  std::string truncatedError;
  const bool acceptedSingleEndBlock = rocket::safe_archive::list(
      archiveTemporary.string(), truncatedNames, truncatedError);
  std::filesystem::remove(archiveTemporary);
  rocket::test::expect(
      archiveListed && archiveNames == std::vector<std::string>{"safe/value.txt"} &&
          !acceptedSingleEndBlock && !truncatedError.empty(),
      "safe archive accepts deterministic ustar and rejects a single-block end marker",
      failures);

  RocketString* binaryText = string(std::string("R\0cket", 6));
  RocketAggregate* binaryBuffer = rocket_std_binary_from_string(binaryText);
  RocketAggregate* decodedBinary = rocket_std_binary_to_string(binaryBuffer);
  auto* decodedBinaryText = static_cast<RocketString*>(
      rocket_rt_aggregate_get_managed(decodedBinary, 0));
  rocket::test::expect(rocket_std_binary_length(binaryBuffer) == 6 &&
                           value(decodedBinaryText) == std::string("R\0cket", 6),
                       "binary buffers preserve embedded zero bytes and valid UTF-8", failures);
  RocketAggregate* encodedU32 = rocket_std_binary_write_u32_le(0x12345678);
  auto* encodedU32Buffer = static_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(encodedU32, 0));
  RocketAggregate* readU32 = rocket_std_binary_read_u32_le(encodedU32Buffer, 0);
  RocketAggregate* shortRead = rocket_std_binary_read_u32_le(encodedU32Buffer, 1);
  rocket::test::expect(rocket_rt_aggregate_tag(readU32) == 0 &&
                           rocket_rt_aggregate_get_int(readU32, 0) == 0x12345678 &&
                           rocket_rt_aggregate_tag(shortRead) == 1,
                       "little-endian reads round-trip and reject truncated input", failures);
  RocketAggregate* middleResult = rocket_std_binary_slice(encodedU32Buffer, 1, 2);
  auto* middleBuffer = static_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(middleResult, 0));
  RocketAggregate* middleValue = rocket_std_binary_read_u16_le(middleBuffer, 0);
  rocket::test::expect(rocket_rt_aggregate_get_int(middleValue, 0) == 0x3456,
                       "binary slices preserve byte order for 16-bit decoding", failures);
  RocketAggregate* invalidByteResult = rocket_std_binary_write_u8(255);
  auto* invalidUtf8Buffer = static_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(invalidByteResult, 0));
  RocketAggregate* byteValue = rocket_std_binary_read_u8(invalidUtf8Buffer, 0);
  RocketAggregate* invalidUtf8 = rocket_std_binary_to_string(invalidUtf8Buffer);
  RocketAggregate* invalidInteger = rocket_std_binary_write_u8(256);
  rocket::test::expect(rocket_rt_aggregate_get_int(byteValue, 0) == 255 &&
                           rocket_rt_aggregate_tag(invalidUtf8) == 1 &&
                           rocket_rt_aggregate_tag(invalidInteger) == 1,
                       "binary conversion validates UTF-8 and unsigned integer ranges", failures);
  rocket_rt_release(invalidInteger);
  rocket_rt_release(invalidUtf8);
  rocket_rt_release(byteValue);
  rocket_rt_release(invalidUtf8Buffer);
  rocket_rt_release(invalidByteResult);
  rocket_rt_release(middleValue);
  rocket_rt_release(middleBuffer);
  rocket_rt_release(middleResult);
  rocket_rt_release(shortRead);
  rocket_rt_release(readU32);
  rocket_rt_release(decodedBinaryText);
  rocket_rt_release(decodedBinary);
  rocket_rt_release(binaryText);

  const std::filesystem::path temporary =
      std::filesystem::current_path() / "rocket_stdlib_test_temp.txt";
  RocketString* path = string(temporary.string());
  RocketString* contents = string("phase-seven");
  RocketAggregate* wrote = rocket_std_file_write_text(path, contents);
  rocket::test::expect(rocket_rt_aggregate_tag(wrote) == 0 &&
                           rocket_std_file_exists(path) == 1,
                       "file.write_text and file.exists operate without exceptions", failures);
  RocketAggregate* read = rocket_std_file_read_text(path);
  auto* readText = static_cast<RocketString*>(rocket_rt_aggregate_get_managed(read, 0));
  rocket::test::expect(value(readText) == "phase-seven",
                       "file.read_text returns the bytes written", failures);
  RocketAggregate* removed = rocket_std_file_remove(path);
  rocket::test::expect(rocket_rt_aggregate_tag(removed) == 0 &&
                           rocket_rt_aggregate_get_bool(removed, 0) == 1,
                       "file.remove returns Ok(true) for an existing file", failures);
  rocket_rt_release(removed);
  rocket_rt_release(readText);
  rocket_rt_release(read);
  rocket_rt_release(wrote);
  rocket_rt_release(contents);
  rocket_rt_release(path);

  const std::filesystem::path binaryTemporary =
      std::filesystem::current_path() / "rocket_stdlib_test_temp.bin";
  RocketString* binaryPath = string(binaryTemporary.string());
  RocketAggregate* wroteBinary = rocket_std_file_write_binary(binaryPath, encodedU32Buffer);
  RocketAggregate* encodedU16 = rocket_std_binary_write_u16_le(0xabcd);
  auto* encodedU16Buffer = static_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(encodedU16, 0));
  RocketAggregate* appendedBinary = rocket_std_file_append_binary(binaryPath, encodedU16Buffer);
  RocketAggregate* readBinary = rocket_std_file_read_binary(binaryPath);
  auto* readBinaryBuffer = static_cast<RocketAggregate*>(
      rocket_rt_aggregate_get_managed(readBinary, 0));
  RocketAggregate* readBinaryValue = rocket_std_binary_read_u32_le(readBinaryBuffer, 0);
  RocketAggregate* readBinaryTail = rocket_std_binary_read_u16_le(readBinaryBuffer, 4);
  rocket::test::expect(rocket_rt_aggregate_tag(wroteBinary) == 0 &&
                           rocket_rt_aggregate_tag(appendedBinary) == 0 &&
                           rocket_std_binary_length(readBinaryBuffer) == 6 &&
                           rocket_rt_aggregate_get_int(readBinaryValue, 0) == 0x12345678 &&
                           rocket_rt_aggregate_get_int(readBinaryTail, 0) == 0xabcd,
                       "binary file write and append preserve exact buffer bytes", failures);
  std::filesystem::remove(binaryTemporary);
  rocket_rt_release(readBinaryTail);
  rocket_rt_release(readBinaryValue);
  rocket_rt_release(readBinaryBuffer);
  rocket_rt_release(readBinary);
  rocket_rt_release(wroteBinary);
  rocket_rt_release(appendedBinary);
  rocket_rt_release(encodedU16Buffer);
  rocket_rt_release(encodedU16);
  rocket_rt_release(binaryPath);
  rocket_rt_release(encodedU32Buffer);
  rocket_rt_release(encodedU32);
  rocket_rt_release(binaryBuffer);

  RocketString* loopback = string("127.0.0.1");
  RocketAggregate* listenerResult = rocket_std_net_tcp_listen(loopback, 0, 4);
  const std::int64_t listenerToken = rocket_rt_aggregate_tag(listenerResult) == 0
                                         ? rocket_rt_aggregate_get_int(listenerResult, 0)
                                         : -1;
  RocketAggregate* portResult = listenerToken >= 0
                                    ? rocket_std_net_local_port(listenerToken)
                                    : nullptr;
  const std::int64_t localPort = portResult && rocket_rt_aggregate_tag(portResult) == 0
                                     ? rocket_rt_aggregate_get_int(portResult, 0)
                                     : -1;
  std::string clientReply;
  std::string clientFailure;
  std::thread localClient([&] {
    rocket::platform_net::Socket socket = rocket::platform_net::invalidSocket;
    if (!rocket::platform_net::connect("127.0.0.1", localPort, 5000, socket,
                                       clientFailure))
      return;
    const std::string request =
        "POST /local HTTP/1.1\r\nHost: localhost\r\nContent-Length: 4\r\n\r\nping";
    std::size_t sent = 0;
    if (rocket::platform_net::send(socket, request, 5000, sent, clientFailure))
      rocket::platform_net::receive(socket, 4096, 5000, clientReply, clientFailure);
    std::string closeError;
    rocket::platform_net::close(socket, closeError);
  });
  RocketAggregate* acceptedResult = listenerToken >= 0
                                        ? rocket_std_net_accept(listenerToken, 5000)
                                        : nullptr;
  const std::int64_t connectionToken =
      acceptedResult && rocket_rt_aggregate_tag(acceptedResult) == 0
          ? rocket_rt_aggregate_get_int(acceptedResult, 0)
          : -1;
  RocketAggregate* requestResult = connectionToken >= 0
                                       ? rocket_std_http_read_request(connectionToken,
                                                                      16384, 5000)
                                       : nullptr;
  RocketAggregate* requestValue =
      requestResult && rocket_rt_aggregate_tag(requestResult) == 0
          ? static_cast<RocketAggregate*>(
                rocket_rt_aggregate_get_managed(requestResult, 0))
          : nullptr;
  RocketString* requestMethod = requestValue
                                    ? static_cast<RocketString*>(
                                          rocket_rt_aggregate_get_managed(requestValue, 0))
                                    : nullptr;
  RocketString* requestPath = requestValue
                                  ? static_cast<RocketString*>(
                                        rocket_rt_aggregate_get_managed(requestValue, 1))
                                  : nullptr;
  RocketAggregate* requestBody = requestValue
                                     ? static_cast<RocketAggregate*>(
                                           rocket_rt_aggregate_get_managed(requestValue, 2))
                                     : nullptr;
  RocketAggregate* decodedRequestBody = requestBody
                                            ? rocket_std_binary_to_string(requestBody)
                                            : nullptr;
  RocketString* decodedRequestText =
      decodedRequestBody && rocket_rt_aggregate_tag(decodedRequestBody) == 0
          ? static_cast<RocketString*>(
                rocket_rt_aggregate_get_managed(decodedRequestBody, 0))
          : nullptr;
  RocketString* responseType = string("text/plain; charset=utf-8");
  RocketString* responseText = string("pong");
  RocketAggregate* responseBody = rocket_std_binary_from_string(responseText);
  RocketAggregate* wroteResponse = connectionToken >= 0
                                       ? rocket_std_http_write_response(
                                             connectionToken, 201, responseType,
                                             responseBody, 5000)
                                       : nullptr;
  RocketAggregate* closedConnection = connectionToken >= 0
                                          ? rocket_std_net_close(connectionToken)
                                          : nullptr;
  RocketAggregate* closedListener = listenerToken >= 0
                                        ? rocket_std_net_cancel(listenerToken)
                                        : nullptr;
  localClient.join();
  rocket::test::expect(
      localPort > 0 && requestMethod && value(requestMethod) == "POST" &&
          requestPath && value(requestPath) == "/local" && decodedRequestText &&
          value(decodedRequestText) == "ping" && wroteResponse &&
          rocket_rt_aggregate_tag(wroteResponse) == 0 && clientFailure.empty() &&
          clientReply.find("HTTP/1.1 201") == 0 &&
          clientReply.ends_with("pong"),
      "bounded TCP and HTTP server APIs round-trip on an isolated loopback socket",
      failures);
  rocket_rt_release(closedListener);
  rocket_rt_release(closedConnection);
  rocket_rt_release(wroteResponse);
  rocket_rt_release(responseBody);
  rocket_rt_release(responseText);
  rocket_rt_release(responseType);
  rocket_rt_release(decodedRequestText);
  rocket_rt_release(decodedRequestBody);
  rocket_rt_release(requestBody);
  rocket_rt_release(requestPath);
  rocket_rt_release(requestMethod);
  rocket_rt_release(requestValue);
  rocket_rt_release(requestResult);
  rocket_rt_release(acceptedResult);
  rocket_rt_release(portResult);
  rocket_rt_release(listenerResult);
  rocket_rt_release(loopback);

  rocket::platform_net::Socket httpListener = rocket::platform_net::invalidSocket;
  std::string httpServerError;
  std::int64_t httpPort = 0;
  const bool startedHttpServer =
      rocket::platform_net::listen("127.0.0.1", 0, 4, httpListener,
                                    httpServerError) &&
      rocket::platform_net::localPort(httpListener, httpPort, httpServerError);
  std::thread httpServer([&] {
    if (!startedHttpServer) return;
    rocket::platform_net::Socket client = rocket::platform_net::invalidSocket;
    if (!rocket::platform_net::accept(httpListener, 5000, client, httpServerError))
      return;
    std::string request;
    if (!rocket::platform_net::receive(client, 16384, 5000, request,
                                        httpServerError))
      return;
    const std::string response =
        "HTTP/1.1 202 Accepted\r\nContent-Length: 9\r\nConnection: close\r\n\r\nclient-ok";
    std::size_t sent = 0;
    rocket::platform_net::send(client, response, 5000, sent, httpServerError);
    std::string ignored;
    rocket::platform_net::close(client, ignored);
  });
  RocketString* httpMethod = string("GET");
  RocketString* httpUrl = string("http://127.0.0.1:" +
                                 std::to_string(httpPort) + "/client");
  RocketString* emptyText = string("");
  RocketAggregate* emptyBody = rocket_std_binary_from_string(emptyText);
  RocketAggregate* httpResult = startedHttpServer
                                    ? rocket_std_http_request(httpMethod, httpUrl,
                                                              emptyBody, 5000)
                                    : nullptr;
  httpServer.join();
  if (startedHttpServer) {
    std::string ignored;
    rocket::platform_net::close(httpListener, ignored);
  }
  RocketAggregate* httpValue = httpResult && rocket_rt_aggregate_tag(httpResult) == 0
                                   ? static_cast<RocketAggregate*>(
                                         rocket_rt_aggregate_get_managed(httpResult, 0))
                                   : nullptr;
  const std::int64_t httpStatus = httpValue
                                      ? rocket_rt_aggregate_get_int(httpValue, 0)
                                      : -1;
  RocketAggregate* httpBody = httpValue
                                  ? static_cast<RocketAggregate*>(
                                        rocket_rt_aggregate_get_managed(httpValue, 1))
                                  : nullptr;
  RocketAggregate* decodedHttpBody = httpBody
                                         ? rocket_std_binary_to_string(httpBody)
                                         : nullptr;
  RocketString* decodedHttpText =
      decodedHttpBody && rocket_rt_aggregate_tag(decodedHttpBody) == 0
          ? static_cast<RocketString*>(
                rocket_rt_aggregate_get_managed(decodedHttpBody, 0))
          : nullptr;
  rocket::test::expect(startedHttpServer && httpServerError.empty() &&
                           httpStatus == 202 && decodedHttpText &&
                           value(decodedHttpText) == "client-ok",
                       "WinHTTP client preserves status and binary body against an isolated server",
                       failures);
  rocket_rt_release(decodedHttpText);
  rocket_rt_release(decodedHttpBody);
  rocket_rt_release(httpBody);
  rocket_rt_release(httpValue);
  rocket_rt_release(httpResult);
  rocket_rt_release(emptyBody);
  rocket_rt_release(emptyText);
  rocket_rt_release(httpUrl);
  rocket_rt_release(httpMethod);

  RocketString* assertionMessage = string("expected failure");
  RocketAggregate* passingAssertion = rocket_std_testing_assert(1, assertionMessage);
  RocketAggregate* failingAssertion = rocket_std_testing_assert(0, assertionMessage);
  RocketString* temporaryPrefix = string("rocket-stdlib-test");
  RocketAggregate* testingTemporary = rocket_std_testing_temp_directory(temporaryPrefix);
  RocketString* testingRoot =
      rocket_rt_aggregate_tag(testingTemporary) == 0
          ? static_cast<RocketString*>(rocket_rt_aggregate_get_managed(testingTemporary, 0))
          : nullptr;
  RocketString* fixtureRelative = string("coverage.json");
  RocketAggregate* fixtureResult = testingRoot
                                       ? rocket_std_testing_fixture_path(
                                             testingRoot, fixtureRelative)
                                       : nullptr;
  RocketString* fixturePath =
      fixtureResult && rocket_rt_aggregate_tag(fixtureResult) == 0
          ? static_cast<RocketString*>(rocket_rt_aggregate_get_managed(fixtureResult, 0))
          : nullptr;
  RocketString* coverageName = string("stdlib.branch");
  RocketAggregate* coverageHitOne = rocket_std_testing_coverage_hit(coverageName);
  RocketAggregate* coverageHitTwo = rocket_std_testing_coverage_hit(coverageName);
  RocketAggregate* coverageWritten = fixturePath
                                         ? rocket_std_testing_coverage_write(fixturePath)
                                         : nullptr;
  std::string coverageText;
  if (fixturePath) {
    std::ifstream coverageInput(value(fixturePath), std::ios::binary);
    coverageText.assign(std::istreambuf_iterator<char>(coverageInput),
                        std::istreambuf_iterator<char>());
  }
  RocketAggregate* testingCleaned = testingRoot
                                        ? rocket_std_testing_cleanup_temp(testingRoot)
                                        : nullptr;
  RocketAggregate* testingCleanedAgain = testingRoot
                                             ? rocket_std_testing_cleanup_temp(testingRoot)
                                             : nullptr;
  rocket::test::expect(
      rocket_rt_aggregate_tag(passingAssertion) == 0 &&
          rocket_rt_aggregate_tag(failingAssertion) == 1 && testingRoot &&
          fixturePath && coverageHitOne && coverageHitTwo && coverageWritten &&
          rocket_rt_aggregate_tag(coverageWritten) == 0 &&
          coverageText.find("\"name\":\"stdlib.branch\",\"hits\":2") !=
              std::string::npos &&
          testingCleaned && rocket_rt_aggregate_tag(testingCleaned) == 0 &&
          !std::filesystem::exists(value(testingRoot)) && testingCleanedAgain &&
          rocket_rt_aggregate_tag(testingCleanedAgain) == 1,
      "testing host boundary handles assertions, fixtures, coverage, and one-shot cleanup",
      failures);
  rocket_rt_release(testingCleanedAgain);
  rocket_rt_release(testingCleaned);
  rocket_rt_release(coverageWritten);
  rocket_rt_release(coverageHitTwo);
  rocket_rt_release(coverageHitOne);
  rocket_rt_release(coverageName);
  rocket_rt_release(fixturePath);
  rocket_rt_release(fixtureResult);
  rocket_rt_release(fixtureRelative);
  rocket_rt_release(testingRoot);
  rocket_rt_release(testingTemporary);
  rocket_rt_release(temporaryPrefix);
  rocket_rt_release(failingAssertion);
  rocket_rt_release(passingAssertion);
  rocket_rt_release(assertionMessage);

  const std::filesystem::path temporaryDirectory =
      std::filesystem::current_path() / "rocket_stdlib_test_directory" / "nested";
  RocketString* directoryPath = string(temporaryDirectory.string());
  RocketAggregate* createdDirectory = rocket_std_file_create_directory(directoryPath);
  rocket::test::expect(rocket_rt_aggregate_tag(createdDirectory) == 0 &&
                           std::filesystem::is_directory(temporaryDirectory),
                       "file.create_directory creates missing parents", failures);
  std::filesystem::remove_all(temporaryDirectory.parent_path());
  rocket_rt_release(createdDirectory);
  rocket_rt_release(directoryPath);

  RocketString* environmentName = string("PATH");
  RocketAggregate* environment = rocket_std_process_environment(environmentName);
  rocket::test::expect(rocket_rt_aggregate_tag(environment) == 0,
                       "process.environment reads an existing variable as Option", failures);
  rocket_rt_release(environment);
  rocket_rt_release(environmentName);
  rocket_std_process_set_arguments(argc, argv);
  RocketAggregate* executablePath = rocket_std_process_executable_path();
  rocket::test::expect(rocket_rt_aggregate_tag(executablePath) == 0,
                       "process.executable_path returns the normalized compiler path", failures);
  rocket_rt_release(executablePath);
  RocketArray* arguments = rocket_std_process_arguments();
  RocketString* lastArgument = argc >= 2
                                   ? rocket_rt_index_string(arguments, argc - 2)
                                   : nullptr;
  rocket::test::expect(argc < 2 ||
                           (rocket_rt_collection_length(arguments) ==
                                static_cast<std::uint64_t>(argc - 1) &&
                            value(lastArgument) == argv[argc - 1]),
                       "process.arguments returns arguments after the executable name", failures);
  rocket_rt_release(lastArgument);
  rocket_rt_release(arguments);
  const std::int64_t before = rocket_std_time_monotonic_milliseconds();
  rocket_std_time_sleep_milliseconds(0);
  rocket::test::expect(rocket_std_time_unix_milliseconds() > 0 &&
                           rocket_std_time_monotonic_milliseconds() >= before,
                       "time clocks and non-negative sleep are available", failures);

  rocket::test::expect(rocket_rt_debug_live_allocations() == 0,
                       "standard-library operations release every managed allocation", failures);
  return rocket::test::finish(failures, "stdlib");
}
