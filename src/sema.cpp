#include "sema.h"

#include <unordered_set>

namespace rocket {

Type SemanticAnalyzer::typeFromName(const std::string& name) {
  if (name == "Int") return Type::Int;
  if (name == "Bool") return Type::Bool;
  if (name == "String") return Type::String;
  if (name == "Unit") return Type::Unit;
  return Type::Invalid;
}

const char* SemanticAnalyzer::typeName(Type type) {
  switch (type) {
  case Type::Int: return "Int";
  case Type::Bool: return "Bool";
  case Type::String: return "String";
  case Type::Unit: return "Unit";
  case Type::Invalid: return "<invalid>";
  }
  return "<invalid>";
}

bool SemanticAnalyzer::analyze() {
  for (const auto& function : module_.functions) {
    if (functions_.contains(function.name)) {
      diagnostics_.error(function.location, "duplicate function '" + function.name + "'");
      continue;
    }
    FunctionType signature;
    for (const auto& parameter : function.parameters) {
      Type type = typeFromName(parameter.typeName);
      if (type == Type::Invalid) diagnostics_.error(parameter.location, "unknown type '" + parameter.typeName + "'");
      signature.parameters.push_back(type);
    }
    signature.result = typeFromName(function.returnType);
    if (signature.result == Type::Invalid)
      diagnostics_.error(function.location, "unknown return type '" + function.returnType + "'");
    functions_.emplace(function.name, std::move(signature));
  }

  auto main = functions_.find("main");
  if (main == functions_.end()) {
    diagnostics_.error({"<module>", 1, 1}, "program must define fn main() -> Int");
  } else if (!main->second.parameters.empty() || main->second.result != Type::Int) {
    diagnostics_.error({"<module>", 1, 1}, "entry point must have signature fn main() -> Int");
  }

  for (const auto& function : module_.functions) analyzeFunction(function);
  return !diagnostics_.hasErrors();
}

void SemanticAnalyzer::analyzeFunction(const Function& function) {
  scopes_.clear();
  scopes_.emplace_back();
  std::unordered_set<std::string> names;
  for (const auto& parameter : function.parameters) {
    if (!names.insert(parameter.name).second)
      diagnostics_.error(parameter.location, "duplicate parameter '" + parameter.name + "'");
    scopes_.back()[parameter.name] = {typeFromName(parameter.typeName), false};
  }
  const Type result = typeFromName(function.returnType);
  analyzeBlock(function.body, result, false);
  if (result != Type::Unit && !definitelyReturns(function.body))
    diagnostics_.error(function.location, "function '" + function.name + "' may finish without returning " + function.returnType);
}

void SemanticAnalyzer::analyzeBlock(const std::vector<std::unique_ptr<Stmt>>& body,
                                    Type returnType, bool nested) {
  if (nested) scopes_.emplace_back();
  for (const auto& statement : body) analyzeStatement(*statement, returnType);
  if (nested) scopes_.pop_back();
}

void SemanticAnalyzer::analyzeStatement(const Stmt& statement, Type returnType) {
  switch (statement.kind) {
  case StmtKind::Binding: {
    const auto& binding = static_cast<const BindingStmt&>(statement);
    const Type type = analyzeExpression(*binding.initializer);
    if (scopes_.back().contains(binding.name))
      diagnostics_.error(binding.location, "duplicate binding '" + binding.name + "' in this scope");
    else scopes_.back()[binding.name] = {type, binding.mutableBinding};
    break;
  }
  case StmtKind::Return: {
    const auto& returned = static_cast<const ReturnStmt&>(statement);
    const Type actual = returned.value ? analyzeExpression(*returned.value) : Type::Unit;
    if (actual != Type::Invalid && returnType != Type::Invalid && actual != returnType)
      diagnostics_.error(returned.location, "return type is " + std::string(typeName(actual)) +
                         ", expected " + typeName(returnType));
    break;
  }
  case StmtKind::Expression:
    analyzeExpression(*static_cast<const ExprStmt&>(statement).expression);
    break;
  case StmtKind::If: {
    const auto& branch = static_cast<const IfStmt&>(statement);
    if (analyzeExpression(*branch.condition) != Type::Bool)
      diagnostics_.error(branch.condition->location, "if condition must have type Bool");
    analyzeBlock(branch.thenBody, returnType, true);
    analyzeBlock(branch.elseBody, returnType, true);
    break;
  }
  case StmtKind::While: {
    const auto& loop = static_cast<const WhileStmt&>(statement);
    if (analyzeExpression(*loop.condition) != Type::Bool)
      diagnostics_.error(loop.condition->location, "while condition must have type Bool");
    analyzeBlock(loop.body, returnType, true);
    break;
  }
  }
}

Type SemanticAnalyzer::analyzeExpression(const Expr& expression) {
  switch (expression.kind) {
  case ExprKind::Integer: return Type::Int;
  case ExprKind::String: return Type::String;
  case ExprKind::Bool: return Type::Bool;
  case ExprKind::Name: {
    const auto& name = static_cast<const LiteralExpr&>(expression).value;
    const Variable* variable = findVariable(name);
    if (!variable) {
      diagnostics_.error(expression.location, "undefined name '" + name + "'");
      return Type::Invalid;
    }
    return variable->type;
  }
  case ExprKind::Unary: {
    const auto& unary = static_cast<const UnaryExpr&>(expression);
    const Type operand = analyzeExpression(*unary.operand);
    if (operand != Type::Int && operand != Type::Invalid)
      diagnostics_.error(expression.location, "unary '-' requires Int");
    return operand == Type::Invalid ? Type::Invalid : Type::Int;
  }
  case ExprKind::Binary: {
    const auto& binary = static_cast<const BinaryExpr&>(expression);
    const Type left = analyzeExpression(*binary.left);
    const Type right = analyzeExpression(*binary.right);
    if (left == Type::Invalid || right == Type::Invalid) return Type::Invalid;
    if (left != right) {
      diagnostics_.error(expression.location, "operator operands have different types");
      return Type::Invalid;
    }
    switch (binary.op) {
    case TokenKind::EqualEqual: case TokenKind::BangEqual:
      return Type::Bool;
    case TokenKind::Less: case TokenKind::LessEqual:
    case TokenKind::Greater: case TokenKind::GreaterEqual:
      if (left != Type::Int) diagnostics_.error(expression.location, "ordering operators require Int operands");
      return Type::Bool;
    case TokenKind::Plus: case TokenKind::Minus: case TokenKind::Star: case TokenKind::Slash:
      if (left != Type::Int) {
        diagnostics_.error(expression.location, "arithmetic operators require Int operands");
        return Type::Invalid;
      }
      return Type::Int;
    default: return Type::Invalid;
    }
  }
  case ExprKind::Call: {
    const auto& call = static_cast<const CallExpr&>(expression);
    if (call.callee->kind != ExprKind::Name) {
      diagnostics_.error(expression.location, "call target must be a function name");
      return Type::Invalid;
    }
    const std::string& name = static_cast<const LiteralExpr&>(*call.callee).value;
    if (name == "print") {
      if (call.arguments.size() != 1)
        diagnostics_.error(expression.location, "print expects exactly one argument");
      for (const auto& argument : call.arguments) analyzeExpression(*argument);
      return Type::Unit;
    }
    auto found = functions_.find(name);
    if (found == functions_.end()) {
      diagnostics_.error(call.callee->location, "unknown function '" + name + "'");
      for (const auto& argument : call.arguments) analyzeExpression(*argument);
      return Type::Invalid;
    }
    if (call.arguments.size() != found->second.parameters.size())
      diagnostics_.error(expression.location, "function '" + name + "' expects " +
                         std::to_string(found->second.parameters.size()) + " argument(s)");
    for (std::size_t i = 0; i < call.arguments.size(); ++i) {
      const Type actual = analyzeExpression(*call.arguments[i]);
      if (i < found->second.parameters.size() && actual != Type::Invalid &&
          actual != found->second.parameters[i])
        diagnostics_.error(call.arguments[i]->location, "argument type is " +
                           std::string(typeName(actual)) + ", expected " +
                           typeName(found->second.parameters[i]));
    }
    return found->second.result;
  }
  }
  return Type::Invalid;
}

const SemanticAnalyzer::Variable* SemanticAnalyzer::findVariable(const std::string& name) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    auto found = scope->find(name);
    if (found != scope->end()) return &found->second;
  }
  return nullptr;
}

bool SemanticAnalyzer::definitelyReturns(const std::vector<std::unique_ptr<Stmt>>& body) const {
  for (const auto& statement : body) {
    if (statement->kind == StmtKind::Return) return true;
    if (statement->kind == StmtKind::If) {
      const auto& branch = static_cast<const IfStmt&>(*statement);
      if (!branch.elseBody.empty() && definitelyReturns(branch.thenBody) &&
          definitelyReturns(branch.elseBody)) return true;
    }
  }
  return false;
}

} // namespace rocket
