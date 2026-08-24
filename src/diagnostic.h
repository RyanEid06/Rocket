#pragma once

#include "token.h"

#include <iostream>
#include <string>
#include <utility>
#include <vector>

namespace rocket {

enum class DiagnosticCode {
  Lexical = 1001,
  Indentation = 1002,
  ResourceLimit = 1003,
  Syntax = 2001,
  ModuleNotFound = 3001,
  ImportCycle = 3002,
  Visibility = 3003,
  ImportAlias = 3004,
  DependencyImport = 3005,
  Type = 4001,
  Name = 4002,
  ControlFlow = 4003,
  PatternMatch = 4004,
  Arity = 4005,
  SendConstraint = 4101,
  ShareConstraint = 4102,
  MoveOnly = 4103,
  ScopedLifetime = 4104,
  AwaitContext = 4105,
  AsyncSuspension = 4106,
  Manifest = 5001,
  Tooling = 5002,
  PackageIntegrity = 5003,
  RegistryAuthorization = 5004,
  DependencyAudit = 5005,
  PackageTransport = 5006,
  PackageCapability = 5007,
  UnknownTarget = 6001,
  UnsupportedTarget = 6002,
  TargetToolchain = 6003,
  HostTargetOperation = 6004,
  TargetManifest = 6005,
  Internal = 9001,
};

inline std::string diagnosticCodeName(DiagnosticCode code) {
  const std::string digits = std::to_string(static_cast<int>(code));
  return "R" + std::string(4 - digits.size(), '0') + digits;
}

struct Diagnostic {
  Location location;
  std::string message;
  DiagnosticCode code = DiagnosticCode::Type;
};

class Diagnostics {
public:
  void error(Location location, std::string message,
             DiagnosticCode code = DiagnosticCode::Type) {
    errors_.push_back({std::move(location), std::move(message), code});
  }

  bool hasErrors() const { return !errors_.empty(); }
  std::size_t size() const { return errors_.size(); }
  const std::vector<Diagnostic>& all() const { return errors_; }

  void print(std::ostream& out = std::cerr) const {
    for (const auto& item : errors_) {
      out << item.location.file << ':' << item.location.line << ':'
          << item.location.column << ": error[" << diagnosticCodeName(item.code)
          << "]: " << item.message << '\n';
    }
  }

private:
  std::vector<Diagnostic> errors_;
};

} // namespace rocket
