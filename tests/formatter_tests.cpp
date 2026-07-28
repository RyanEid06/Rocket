#include "formatter.h"
#include "test_support.h"

#include <string>

int main() {
  int failures = 0;
  const std::string source =
      "import  std.string   \r\n"
      "\r\n"
      "# module comment   \r\n"
      "fn  main( )->Int: # entry   \r\n"
      "    let  values : Array [ Int ] = [ 1,2, 3 ]\r\n"
      "    let negative=-1\r\n"
      "    if  not false and values [0]==1:\r\n"
      "        print ( string.concat(\"A#\",\"B\") ) # result\r\n"
      "    return negative+2\r\n";
  const std::string expected =
      "import std.string\n"
      "\n"
      "# module comment\n"
      "fn main() -> Int:  # entry\n"
      "    let values: Array[Int] = [1, 2, 3]\n"
      "    let negative = -1\n"
      "    if not false and values[0] == 1:\n"
      "        print(string.concat(\"A#\", \"B\"))  # result\n"
      "    return negative + 2\n";

  rocket::Diagnostics diagnostics;
  auto formatted = rocket::formatSource("format.rocket", source, diagnostics);
  rocket::test::expect(formatted.has_value() && *formatted == expected,
                       "formatter canonicalizes spacing, indentation, comments, and newlines",
                       failures);
  if (formatted) {
    rocket::Diagnostics secondDiagnostics;
    auto second = rocket::formatSource("format.rocket", *formatted, secondDiagnostics);
    rocket::test::expect(second.has_value() && *second == *formatted,
                         "formatter is idempotent", failures);
  }

  rocket::Diagnostics invalidDiagnostics;
  auto invalid = rocket::formatSource("invalid.rocket", "fn main() -> Int:\n\treturn 0\n",
                                      invalidDiagnostics);
  rocket::test::expect(!invalid.has_value() && invalidDiagnostics.hasErrors() &&
                           invalidDiagnostics.all().front().code ==
                               rocket::DiagnosticCode::Indentation,
                       "formatter refuses lexically invalid indentation with a stable code",
                       failures);
  return rocket::test::finish(failures, "formatter");
}
