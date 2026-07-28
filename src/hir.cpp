#include "hir.h"

#include <charconv>
#include <cstdint>
#include <unordered_set>
#include <utility>

namespace rocket {

SymbolId HirLowerer::addSymbol(SymbolKind kind, const std::string& name, Type type,
                               bool mutableBinding, const Location& location,
                               std::vector<Type> parameterTypes) {
  const SymbolId id = static_cast<SymbolId>(hir_.symbols.size());
  hir_.symbols.push_back({id, kind, name, type, mutableBinding, location,
                          std::move(parameterTypes)});
  return id;
}

std::optional<HirModule> HirLowerer::lower() {
  hir_ = {};
  functions_.clear();
  functionSymbols_.clear();

  const SymbolId print = addSymbol(SymbolKind::BuiltinFunction, "print", Type::Unit, false,
                                   {"<builtin>", 1, 1});
  functions_.emplace("print", print);

  for (const auto& function : ast_.functions) {
    std::vector<Type> parameters;
    for (const auto& parameter : function.parameters) {
      const Type type = typeFromName(parameter.typeName);
      if (type == Type::Invalid)
        diagnostics_.error(parameter.location, "unknown type '" + parameter.typeName + "'");
      parameters.push_back(type);
    }
    const Type result = typeFromName(function.returnType);
    if (result == Type::Invalid)
      diagnostics_.error(function.location, "unknown return type '" + function.returnType + "'");

    const SymbolId symbol = addSymbol(SymbolKind::Function, function.name, result, false,
                                      function.location, parameters);
    functionSymbols_.push_back(symbol);
    if (functions_.contains(function.name)) {
      diagnostics_.error(function.location, "duplicate function '" + function.name + "'");
    } else {
      functions_.emplace(function.name, symbol);
    }
  }

  auto main = functions_.find("main");
  if (main == functions_.end()) {
    diagnostics_.error({"<module>", 1, 1}, "program must define fn main() -> Int");
  } else {
    const auto& signature = hir_.symbol(main->second);
    if (!signature.parameterTypes.empty() || signature.type != Type::Int)
      diagnostics_.error({"<module>", 1, 1},
                         "entry point must have signature fn main() -> Int");
  }

  for (std::size_t i = 0; i < ast_.functions.size(); ++i)
    hir_.functions.push_back(lowerFunction(ast_.functions[i], functionSymbols_[i]));

  if (diagnostics_.hasErrors()) return std::nullopt;
  return std::move(hir_);
}

HirFunction HirLowerer::lowerFunction(const Function& function, SymbolId symbol) {
  scopes_.clear();
  scopes_.emplace_back();
  loopDepth_ = 0;

  HirFunction result;
  result.symbol = symbol;
  result.location = function.location;
  result.result = typeFromName(function.returnType);

  std::unordered_set<std::string> names;
  for (const auto& parameter : function.parameters) {
    const Type type = typeFromName(parameter.typeName);
    const SymbolId parameterSymbol = addSymbol(SymbolKind::Parameter, parameter.name, type, false,
                                                parameter.location);
    result.parameters.push_back({parameterSymbol});
    if (!names.insert(parameter.name).second) {
      diagnostics_.error(parameter.location, "duplicate parameter '" + parameter.name + "'");
    } else {
      scopes_.back().emplace(parameter.name, parameterSymbol);
    }
  }

  result.body = lowerBlock(function.body, result.result, false);
  if (result.result != Type::Unit && !definitelyReturns(result.body))
    diagnostics_.error(function.location, "function '" + function.name +
                                           "' may finish without returning " + function.returnType);
  return result;
}

HirBlock HirLowerer::lowerBlock(const std::vector<std::unique_ptr<Stmt>>& body,
                                Type returnType, bool nested) {
  if (nested) scopes_.emplace_back();
  HirBlock result;
  for (const auto& statement : body)
    result.push_back(lowerStatement(*statement, returnType));
  if (nested) scopes_.pop_back();
  return result;
}

std::unique_ptr<HirStmt> HirLowerer::lowerStatement(const Stmt& statement, Type returnType) {
  switch (statement.kind) {
  case StmtKind::Binding: {
    const auto& binding = static_cast<const BindingStmt&>(statement);
    auto initializer = lowerExpression(*binding.initializer);
    const SymbolId symbol = addSymbol(SymbolKind::Local, binding.name, initializer->type,
                                      binding.mutableBinding, binding.location);
    if (scopes_.back().contains(binding.name)) {
      diagnostics_.error(binding.location,
                         "duplicate binding '" + binding.name + "' in this scope");
    } else {
      scopes_.back().emplace(binding.name, symbol);
    }
    return std::make_unique<HirBindingStmt>(binding.location, symbol, std::move(initializer));
  }
  case StmtKind::Assignment: {
    const auto& assignment = static_cast<const AssignmentStmt&>(statement);
    const SymbolId target = findVariable(assignment.name);
    auto value = lowerExpression(*assignment.value);
    if (target == InvalidSymbol) {
      diagnostics_.error(assignment.location,
                         "cannot assign to undefined name '" + assignment.name + "'");
    } else {
      const auto& symbol = hir_.symbol(target);
      if (!symbol.mutableBinding) {
        diagnostics_.error(assignment.location,
                           "cannot assign to immutable binding '" + assignment.name + "'");
      } else if (value->type != Type::Invalid && value->type != symbol.type) {
        diagnostics_.error(assignment.location,
                           "assignment type is " + std::string(typeName(value->type)) +
                               ", expected " + typeName(symbol.type));
      }
    }
    return std::make_unique<HirAssignmentStmt>(assignment.location, target, std::move(value));
  }
  case StmtKind::Return: {
    const auto& returned = static_cast<const ReturnStmt&>(statement);
    auto value = returned.value ? lowerExpression(*returned.value) : nullptr;
    const Type actual = value ? value->type : Type::Unit;
    if (actual != Type::Invalid && returnType != Type::Invalid && actual != returnType)
      diagnostics_.error(returned.location,
                         "return type is " + std::string(typeName(actual)) + ", expected " +
                             typeName(returnType));
    return std::make_unique<HirReturnStmt>(returned.location, std::move(value));
  }
  case StmtKind::Expression: {
    const auto& expression = static_cast<const ExprStmt&>(statement);
    return std::make_unique<HirExprStmt>(expression.location,
                                         lowerExpression(*expression.expression));
  }
  case StmtKind::If: {
    const auto& branch = static_cast<const IfStmt&>(statement);
    auto condition = lowerExpression(*branch.condition);
    if (condition->type != Type::Bool)
      diagnostics_.error(branch.condition->location, "if condition must have type Bool");
    auto thenBody = lowerBlock(branch.thenBody, returnType, true);
    auto elseBody = lowerBlock(branch.elseBody, returnType, true);
    return std::make_unique<HirIfStmt>(branch.location, std::move(condition),
                                       std::move(thenBody), std::move(elseBody));
  }
  case StmtKind::While: {
    const auto& loop = static_cast<const WhileStmt&>(statement);
    auto condition = lowerExpression(*loop.condition);
    if (condition->type != Type::Bool)
      diagnostics_.error(loop.condition->location, "while condition must have type Bool");
    ++loopDepth_;
    auto body = lowerBlock(loop.body, returnType, true);
    --loopDepth_;
    return std::make_unique<HirWhileStmt>(loop.location, std::move(condition), std::move(body));
  }
  case StmtKind::For: {
    const auto& loop = static_cast<const ForStmt&>(statement);
    auto start = lowerExpression(*loop.start);
    auto end = lowerExpression(*loop.end);
    if (start->type != Type::Invalid && start->type != Type::Int)
      diagnostics_.error(loop.start->location, "range start must have type Int");
    if (end->type != Type::Invalid && end->type != Type::Int)
      diagnostics_.error(loop.end->location, "range end must have type Int");

    scopes_.emplace_back();
    const SymbolId variable = addSymbol(SymbolKind::LoopVariable, loop.name, Type::Int, false,
                                        loop.location);
    scopes_.back().emplace(loop.name, variable);
    ++loopDepth_;
    auto body = lowerBlock(loop.body, returnType, false);
    --loopDepth_;
    scopes_.pop_back();
    return std::make_unique<HirForStmt>(loop.location, variable, std::move(start),
                                        std::move(end), std::move(body));
  }
  case StmtKind::Break:
  case StmtKind::Continue: {
    if (loopDepth_ == 0)
      diagnostics_.error(statement.location, statement.kind == StmtKind::Break
                                                 ? "'break' is only valid inside a loop"
                                                 : "'continue' is only valid inside a loop");
    return std::make_unique<HirLoopControlStmt>(
        statement.kind == StmtKind::Break ? HirStmtKind::Break : HirStmtKind::Continue,
        statement.location);
  }
  }
  return std::make_unique<HirLoopControlStmt>(HirStmtKind::Break, statement.location);
}

std::unique_ptr<HirExpr> HirLowerer::lowerExpression(const Expr& expression) {
  switch (expression.kind) {
  case ExprKind::Integer: {
    const auto& literal = static_cast<const LiteralExpr&>(expression).value;
    std::int64_t parsed = 0;
    const auto result = std::from_chars(literal.data(), literal.data() + literal.size(), parsed);
    if (result.ec == std::errc::result_out_of_range)
      diagnostics_.error(expression.location, "Int literal is outside the signed 64-bit range");
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Int,
                                            literal);
  }
  case ExprKind::Float:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Float,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Character:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Char,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::String:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::String,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Bool:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Bool,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Name: {
    const auto& name = static_cast<const LiteralExpr&>(expression).value;
    const SymbolId symbol = findVariable(name);
    if (symbol == InvalidSymbol) {
      diagnostics_.error(expression.location, "undefined name '" + name + "'");
      return std::make_unique<HirNameExpr>(expression.location, Type::Invalid, symbol);
    }
    return std::make_unique<HirNameExpr>(expression.location, hir_.symbol(symbol).type, symbol);
  }
  case ExprKind::Unary: {
    const auto& unary = static_cast<const UnaryExpr&>(expression);
    auto operand = lowerExpression(*unary.operand);
    Type result = operand->type;
    if (unary.op == TokenKind::KwNot) {
      if (operand->type != Type::Bool && operand->type != Type::Invalid)
        diagnostics_.error(expression.location, "'not' requires a Bool operand");
      result = operand->type == Type::Invalid ? Type::Invalid : Type::Bool;
    } else if (operand->type != Type::Int && operand->type != Type::Float &&
               operand->type != Type::Invalid) {
      diagnostics_.error(expression.location, "unary '-' requires Int or Float");
    }
    return std::make_unique<HirUnaryExpr>(expression.location, result, unary.op,
                                          std::move(operand));
  }
  case ExprKind::Binary: {
    const auto& binary = static_cast<const BinaryExpr&>(expression);
    auto left = lowerExpression(*binary.left);
    auto right = lowerExpression(*binary.right);
    Type result = Type::Invalid;
    if (left->type != Type::Invalid && right->type != Type::Invalid) {
      if (left->type != right->type) {
        diagnostics_.error(expression.location, "operator operands have different types");
      } else {
        switch (binary.op) {
        case TokenKind::KwAnd:
        case TokenKind::KwOr:
          if (left->type != Type::Bool)
            diagnostics_.error(expression.location, "logical operators require Bool operands");
          else result = Type::Bool;
          break;
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
          if (isCollectionType(left->type))
            diagnostics_.error(expression.location,
                               "Array and Slice equality is not available yet");
          else
            result = Type::Bool;
          break;
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
          if (left->type != Type::Int && left->type != Type::Float)
            diagnostics_.error(expression.location,
                               "ordering operators require Int or Float operands");
          result = Type::Bool;
          break;
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
          if (left->type != Type::Int && left->type != Type::Float)
            diagnostics_.error(expression.location,
                               "arithmetic operators require Int or Float operands");
          else result = left->type;
          break;
        default: break;
        }
      }
    }
    return std::make_unique<HirBinaryExpr>(expression.location, result, std::move(left),
                                           binary.op, std::move(right));
  }
  case ExprKind::Call: {
    const auto& call = static_cast<const CallExpr&>(expression);
    std::vector<std::unique_ptr<HirExpr>> arguments;
    for (const auto& argument : call.arguments)
      arguments.push_back(lowerExpression(*argument));

    if (call.callee->kind != ExprKind::Name) {
      diagnostics_.error(expression.location, "call target must be a function name");
      return std::make_unique<HirCallExpr>(expression.location, Type::Invalid, InvalidSymbol,
                                           std::move(arguments));
    }
    const std::string& name = static_cast<const LiteralExpr&>(*call.callee).value;
    auto found = functions_.find(name);
    if (found == functions_.end()) {
      diagnostics_.error(call.callee->location, "unknown function '" + name + "'");
      return std::make_unique<HirCallExpr>(expression.location, Type::Invalid, InvalidSymbol,
                                           std::move(arguments));
    }

    const SymbolId callee = found->second;
    const auto& signature = hir_.symbol(callee);
    if (signature.kind == SymbolKind::BuiltinFunction) {
      if (arguments.size() != 1)
        diagnostics_.error(expression.location, "print expects exactly one argument");
      else if (isCollectionType(arguments[0]->type))
        diagnostics_.error(arguments[0]->location,
                           "print does not accept Array or Slice values");
      return std::make_unique<HirCallExpr>(expression.location, Type::Unit, callee,
                                           std::move(arguments));
    }

    if (arguments.size() != signature.parameterTypes.size())
      diagnostics_.error(expression.location,
                         "function '" + name + "' expects " +
                             std::to_string(signature.parameterTypes.size()) + " argument(s)");
    for (std::size_t i = 0; i < arguments.size() && i < signature.parameterTypes.size(); ++i) {
      if (arguments[i]->type != Type::Invalid &&
          arguments[i]->type != signature.parameterTypes[i])
        diagnostics_.error(arguments[i]->location,
                           "argument type is " + std::string(typeName(arguments[i]->type)) +
                               ", expected " + typeName(signature.parameterTypes[i]));
    }
    return std::make_unique<HirCallExpr>(expression.location, signature.type, callee,
                                         std::move(arguments));
  }
  case ExprKind::Array: {
    const auto& array = static_cast<const ArrayExpr&>(expression);
    std::vector<std::unique_ptr<HirExpr>> elements;
    for (const auto& element : array.elements)
      elements.push_back(lowerExpression(*element));
    if (elements.empty()) {
      diagnostics_.error(expression.location,
                         "empty Array literals need an element type (not yet supported)");
      return std::make_unique<HirArrayExpr>(expression.location, Type::Invalid,
                                            std::move(elements));
    }
    const Type elementType = elements.front()->type;
    const Type result = arrayType(elementType);
    if (result == Type::Invalid && elementType != Type::Invalid)
      diagnostics_.error(expression.location,
                         "Array elements must be Int, Float, Bool, Char, or String");
    for (std::size_t index = 1; index < elements.size(); ++index) {
      if (elements[index]->type != Type::Invalid && elementType != Type::Invalid &&
          elements[index]->type != elementType)
        diagnostics_.error(elements[index]->location,
                           "Array literal elements must have one type; found " +
                               std::string(typeName(elements[index]->type)) +
                               ", expected " + typeName(elementType));
    }
    return std::make_unique<HirArrayExpr>(expression.location, result, std::move(elements));
  }
  case ExprKind::Index: {
    const auto& index = static_cast<const IndexExpr&>(expression);
    auto collection = lowerExpression(*index.collection);
    auto offset = lowerExpression(*index.index);
    if (collection->type != Type::Invalid && !isCollectionType(collection->type))
      diagnostics_.error(index.collection->location,
                         "indexing requires an Array or Slice value");
    if (offset->type != Type::Invalid && offset->type != Type::Int)
      diagnostics_.error(index.index->location, "collection index must have type Int");
    const Type result = collectionElementType(collection->type);
    return std::make_unique<HirIndexExpr>(expression.location, result,
                                          std::move(collection), std::move(offset));
  }
  case ExprKind::Slice: {
    const auto& slice = static_cast<const SliceExpr&>(expression);
    auto collection = lowerExpression(*slice.collection);
    auto start = lowerExpression(*slice.start);
    auto end = lowerExpression(*slice.end);
    if (collection->type != Type::Invalid && !isCollectionType(collection->type))
      diagnostics_.error(slice.collection->location,
                         "slicing requires an Array or Slice value");
    if (start->type != Type::Invalid && start->type != Type::Int)
      diagnostics_.error(slice.start->location, "slice start must have type Int");
    if (end->type != Type::Invalid && end->type != Type::Int)
      diagnostics_.error(slice.end->location, "slice end must have type Int");
    const Type result = sliceType(collectionElementType(collection->type));
    return std::make_unique<HirSliceExpr>(expression.location, result,
                                          std::move(collection), std::move(start),
                                          std::move(end));
  }
  }
  return std::make_unique<HirLiteralExpr>(expression.location, Type::Invalid, "0");
}

SymbolId HirLowerer::findVariable(const std::string& name) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    auto found = scope->find(name);
    if (found != scope->end()) return found->second;
  }
  return InvalidSymbol;
}

bool HirLowerer::definitelyReturns(const HirBlock& body) const {
  for (const auto& statement : body) {
    if (statement->kind == HirStmtKind::Return) return true;
    if (statement->kind == HirStmtKind::If) {
      const auto& branch = static_cast<const HirIfStmt&>(*statement);
      if (!branch.elseBody.empty() && definitelyReturns(branch.thenBody) &&
          definitelyReturns(branch.elseBody))
        return true;
    }
  }
  return false;
}

} // namespace rocket
