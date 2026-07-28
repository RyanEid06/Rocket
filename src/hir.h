#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "type.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace rocket {

using SymbolId = std::uint32_t;
inline constexpr SymbolId InvalidSymbol = static_cast<SymbolId>(-1);

enum class SymbolKind { BuiltinFunction, Function, Parameter, Local, LoopVariable };

struct HirSymbol {
  SymbolId id = InvalidSymbol;
  SymbolKind kind = SymbolKind::Local;
  std::string name;
  Type type = Type::Invalid;
  bool mutableBinding = false;
  Location location;
  std::vector<Type> parameterTypes;
};

enum class HirExprKind { Literal, Name, Unary, Binary, Call, Array, Index, Slice };

struct HirExpr {
  HirExpr(HirExprKind kind, Location location, Type type)
      : kind(kind), location(std::move(location)), type(type) {}
  virtual ~HirExpr() = default;
  HirExprKind kind;
  Location location;
  Type type;
};

struct HirLiteralExpr final : HirExpr {
  HirLiteralExpr(Location location, Type type, std::string value)
      : HirExpr(HirExprKind::Literal, std::move(location), type), value(std::move(value)) {}
  std::string value;
};

struct HirNameExpr final : HirExpr {
  HirNameExpr(Location location, Type type, SymbolId symbol)
      : HirExpr(HirExprKind::Name, std::move(location), type), symbol(symbol) {}
  SymbolId symbol;
};

struct HirUnaryExpr final : HirExpr {
  HirUnaryExpr(Location location, Type type, TokenKind op, std::unique_ptr<HirExpr> operand)
      : HirExpr(HirExprKind::Unary, std::move(location), type), op(op),
        operand(std::move(operand)) {}
  TokenKind op;
  std::unique_ptr<HirExpr> operand;
};

struct HirBinaryExpr final : HirExpr {
  HirBinaryExpr(Location location, Type type, std::unique_ptr<HirExpr> left, TokenKind op,
                std::unique_ptr<HirExpr> right)
      : HirExpr(HirExprKind::Binary, std::move(location), type), left(std::move(left)),
        op(op), right(std::move(right)) {}
  std::unique_ptr<HirExpr> left;
  TokenKind op;
  std::unique_ptr<HirExpr> right;
};

struct HirCallExpr final : HirExpr {
  HirCallExpr(Location location, Type type, SymbolId callee,
              std::vector<std::unique_ptr<HirExpr>> arguments)
      : HirExpr(HirExprKind::Call, std::move(location), type), callee(callee),
        arguments(std::move(arguments)) {}
  SymbolId callee;
  std::vector<std::unique_ptr<HirExpr>> arguments;
};

struct HirArrayExpr final : HirExpr {
  HirArrayExpr(Location location, Type type, std::vector<std::unique_ptr<HirExpr>> elements)
      : HirExpr(HirExprKind::Array, std::move(location), type),
        elements(std::move(elements)) {}
  std::vector<std::unique_ptr<HirExpr>> elements;
};

struct HirIndexExpr final : HirExpr {
  HirIndexExpr(Location location, Type type, std::unique_ptr<HirExpr> collection,
               std::unique_ptr<HirExpr> index)
      : HirExpr(HirExprKind::Index, std::move(location), type),
        collection(std::move(collection)), index(std::move(index)) {}
  std::unique_ptr<HirExpr> collection;
  std::unique_ptr<HirExpr> index;
};

struct HirSliceExpr final : HirExpr {
  HirSliceExpr(Location location, Type type, std::unique_ptr<HirExpr> collection,
               std::unique_ptr<HirExpr> start, std::unique_ptr<HirExpr> end)
      : HirExpr(HirExprKind::Slice, std::move(location), type),
        collection(std::move(collection)), start(std::move(start)), end(std::move(end)) {}
  std::unique_ptr<HirExpr> collection;
  std::unique_ptr<HirExpr> start;
  std::unique_ptr<HirExpr> end;
};

enum class HirStmtKind { Binding, Assignment, Return, Expression, If, While, For, Break, Continue };

struct HirStmt {
  HirStmt(HirStmtKind kind, Location location) : kind(kind), location(std::move(location)) {}
  virtual ~HirStmt() = default;
  HirStmtKind kind;
  Location location;
};

using HirBlock = std::vector<std::unique_ptr<HirStmt>>;

struct HirBindingStmt final : HirStmt {
  HirBindingStmt(Location location, SymbolId symbol, std::unique_ptr<HirExpr> initializer)
      : HirStmt(HirStmtKind::Binding, std::move(location)), symbol(symbol),
        initializer(std::move(initializer)) {}
  SymbolId symbol;
  std::unique_ptr<HirExpr> initializer;
};

struct HirAssignmentStmt final : HirStmt {
  HirAssignmentStmt(Location location, SymbolId target, std::unique_ptr<HirExpr> value)
      : HirStmt(HirStmtKind::Assignment, std::move(location)), target(target),
        value(std::move(value)) {}
  SymbolId target;
  std::unique_ptr<HirExpr> value;
};

struct HirReturnStmt final : HirStmt {
  HirReturnStmt(Location location, std::unique_ptr<HirExpr> value)
      : HirStmt(HirStmtKind::Return, std::move(location)), value(std::move(value)) {}
  std::unique_ptr<HirExpr> value;
};

struct HirExprStmt final : HirStmt {
  HirExprStmt(Location location, std::unique_ptr<HirExpr> expression)
      : HirStmt(HirStmtKind::Expression, std::move(location)), expression(std::move(expression)) {}
  std::unique_ptr<HirExpr> expression;
};

struct HirIfStmt final : HirStmt {
  HirIfStmt(Location location, std::unique_ptr<HirExpr> condition, HirBlock thenBody,
            HirBlock elseBody)
      : HirStmt(HirStmtKind::If, std::move(location)), condition(std::move(condition)),
        thenBody(std::move(thenBody)), elseBody(std::move(elseBody)) {}
  std::unique_ptr<HirExpr> condition;
  HirBlock thenBody;
  HirBlock elseBody;
};

struct HirWhileStmt final : HirStmt {
  HirWhileStmt(Location location, std::unique_ptr<HirExpr> condition, HirBlock body)
      : HirStmt(HirStmtKind::While, std::move(location)), condition(std::move(condition)),
        body(std::move(body)) {}
  std::unique_ptr<HirExpr> condition;
  HirBlock body;
};

struct HirForStmt final : HirStmt {
  HirForStmt(Location location, SymbolId variable, std::unique_ptr<HirExpr> start,
             std::unique_ptr<HirExpr> end, HirBlock body)
      : HirStmt(HirStmtKind::For, std::move(location)), variable(variable),
        start(std::move(start)), end(std::move(end)), body(std::move(body)) {}
  SymbolId variable;
  std::unique_ptr<HirExpr> start;
  std::unique_ptr<HirExpr> end;
  HirBlock body;
};

struct HirLoopControlStmt final : HirStmt {
  HirLoopControlStmt(HirStmtKind kind, Location location) : HirStmt(kind, std::move(location)) {}
};

struct HirParameter { SymbolId symbol = InvalidSymbol; };

struct HirFunction {
  SymbolId symbol = InvalidSymbol;
  Location location;
  std::vector<HirParameter> parameters;
  Type result = Type::Invalid;
  HirBlock body;
};

struct HirModule {
  std::vector<HirSymbol> symbols;
  std::vector<HirFunction> functions;

  const HirSymbol& symbol(SymbolId id) const { return symbols.at(id); }
};

class HirLowerer {
public:
  HirLowerer(const Module& module, Diagnostics& diagnostics)
      : ast_(module), diagnostics_(diagnostics) {}
  std::optional<HirModule> lower();

private:
  using Scope = std::unordered_map<std::string, SymbolId>;

  SymbolId addSymbol(SymbolKind kind, const std::string& name, Type type, bool mutableBinding,
                     const Location& location, std::vector<Type> parameterTypes = {});
  HirFunction lowerFunction(const Function& function, SymbolId symbol);
  HirBlock lowerBlock(const std::vector<std::unique_ptr<Stmt>>& body, Type returnType, bool nested);
  std::unique_ptr<HirStmt> lowerStatement(const Stmt& statement, Type returnType);
  std::unique_ptr<HirExpr> lowerExpression(const Expr& expression);
  SymbolId findVariable(const std::string& name) const;
  bool definitelyReturns(const HirBlock& body) const;

  const Module& ast_;
  Diagnostics& diagnostics_;
  HirModule hir_;
  std::unordered_map<std::string, SymbolId> functions_;
  std::vector<SymbolId> functionSymbols_;
  std::vector<Scope> scopes_;
  int loopDepth_ = 0;
};

} // namespace rocket
