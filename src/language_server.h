#pragma once

#include <iosfwd>
namespace rocket {

class LanguageServer {
public:
  LanguageServer(std::istream& input, std::ostream& output, std::ostream& log)
      : input_(input), output_(output), log_(log) {}

  int run();

private:
  std::istream& input_;
  std::ostream& output_;
  std::ostream& log_;
};

} // namespace rocket
