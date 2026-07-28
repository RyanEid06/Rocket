#pragma once

#include "ast.h"
#include "diagnostic.h"

#include <string>
#include <unordered_map>
#include <vector>

namespace rocket {

enum class Type { Invalid, Int, Bool, String, Unit };

struct FunctionType {
  std::vector<Type> parameters;
  Type result = Type::Invalid;
};

class SemanticAnalyzer {
public:
  SemanticAnalyzer(const Module& module, Diagnostics& diagnostics)
      : module_(module), diagnostics_(diagnostics) {}
  bool analyze();

  static Type typeFromName(const std::string& name);
  static const char* typeName(Type type);

private:
  struct Variable { Type type; bool mutableBinding; };
  using Scope = std::unordered_map<std::string, Variable>;

  void analyzeFunction(const Function& function);
  void analyzeBlock(const std::vector<std::unique_ptr<Stmt>>& body, Type returnType, bool nested);
  void analyzeStatement(const Stmt& statement, Type returnType);
  Type analyzeExpression(const Expr& expression);
  const Variable* findVariable(const std::string& name) const;
  bool definitelyReturns(const std::vector<std::unique_ptr<Stmt>>& body) const;

  const Module& module_;
  Diagnostics& diagnostics_;
  std::unordered_map<std::string, FunctionType> functions_;
  std::vector<Scope> scopes_;
};

} // namespace rocket
