#pragma once

#include "token.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace rocket {

struct Diagnostic {
  Location location;
  std::string message;
};

class Diagnostics {
public:
  void error(Location location, std::string message) {
    errors_.push_back({std::move(location), std::move(message)});
  }

  bool hasErrors() const { return !errors_.empty(); }
  std::size_t size() const { return errors_.size(); }
  const std::vector<Diagnostic>& all() const { return errors_; }

  void print(std::ostream& out = std::cerr) const {
    for (const auto& item : errors_) {
      out << item.location.file << ':' << item.location.line << ':'
          << item.location.column << ": error: " << item.message << '\n';
    }
  }

private:
  std::vector<Diagnostic> errors_;
};

} // namespace rocket
