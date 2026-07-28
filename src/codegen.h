#pragma once

#include "ast.h"

#include <ostream>
#include <string>

namespace rocket {

class BootstrapCodeGenerator {
public:
  explicit BootstrapCodeGenerator(const Module& module) : module_(module) {}
  std::string generate() const;

private:
  void emitFunction(std::ostream& out, const Function& function) const;
  void emitBlock(std::ostream& out, const std::vector<std::unique_ptr<Stmt>>& body,
                 int indentation) const;
  void emitStatement(std::ostream& out, const Stmt& statement, int indentation) const;
  void emitExpression(std::ostream& out, const Expr& expression) const;
  static std::string cppType(const std::string& type);
  static std::string escaped(const std::string& text);
  static std::string functionName(const std::string& name);
  static std::string variableName(const std::string& name);

  const Module& module_;
};

} // namespace rocket
