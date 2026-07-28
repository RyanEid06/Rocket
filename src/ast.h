#pragma once

#include "token.h"

#include <memory>
#include <string>
#include <vector>

namespace rocket {

enum class ExprKind {
  Integer, Float, Character, String, Bool, Name, Unary, Binary, Call, Array, Index, Slice
};

struct Expr {
  explicit Expr(ExprKind kind, Location location) : kind(kind), location(std::move(location)) {}
  virtual ~Expr() = default;
  ExprKind kind;
  Location location;
};

struct LiteralExpr final : Expr {
  LiteralExpr(ExprKind kind, Location location, std::string value)
      : Expr(kind, std::move(location)), value(std::move(value)) {}
  std::string value;
};

struct UnaryExpr final : Expr {
  UnaryExpr(Location location, TokenKind op, std::unique_ptr<Expr> operand)
      : Expr(ExprKind::Unary, std::move(location)), op(op), operand(std::move(operand)) {}
  TokenKind op;
  std::unique_ptr<Expr> operand;
};

struct BinaryExpr final : Expr {
  BinaryExpr(Location location, std::unique_ptr<Expr> left, TokenKind op,
             std::unique_ptr<Expr> right)
      : Expr(ExprKind::Binary, std::move(location)), left(std::move(left)), op(op),
        right(std::move(right)) {}
  std::unique_ptr<Expr> left;
  TokenKind op;
  std::unique_ptr<Expr> right;
};

struct CallExpr final : Expr {
  CallExpr(Location location, std::unique_ptr<Expr> callee,
           std::vector<std::unique_ptr<Expr>> arguments)
      : Expr(ExprKind::Call, std::move(location)), callee(std::move(callee)),
        arguments(std::move(arguments)) {}
  std::unique_ptr<Expr> callee;
  std::vector<std::unique_ptr<Expr>> arguments;
};

struct ArrayExpr final : Expr {
  ArrayExpr(Location location, std::vector<std::unique_ptr<Expr>> elements)
      : Expr(ExprKind::Array, std::move(location)), elements(std::move(elements)) {}
  std::vector<std::unique_ptr<Expr>> elements;
};

struct IndexExpr final : Expr {
  IndexExpr(Location location, std::unique_ptr<Expr> collection,
            std::unique_ptr<Expr> index)
      : Expr(ExprKind::Index, std::move(location)), collection(std::move(collection)),
        index(std::move(index)) {}
  std::unique_ptr<Expr> collection;
  std::unique_ptr<Expr> index;
};

struct SliceExpr final : Expr {
  SliceExpr(Location location, std::unique_ptr<Expr> collection,
            std::unique_ptr<Expr> start, std::unique_ptr<Expr> end)
      : Expr(ExprKind::Slice, std::move(location)), collection(std::move(collection)),
        start(std::move(start)), end(std::move(end)) {}
  std::unique_ptr<Expr> collection;
  std::unique_ptr<Expr> start;
  std::unique_ptr<Expr> end;
};

enum class StmtKind { Binding, Assignment, Return, Expression, If, While, For, Break, Continue };

struct Stmt {
  explicit Stmt(StmtKind kind, Location location) : kind(kind), location(std::move(location)) {}
  virtual ~Stmt() = default;
  StmtKind kind;
  Location location;
};

struct BindingStmt final : Stmt {
  BindingStmt(Location location, bool mutableBinding, std::string name,
              std::unique_ptr<Expr> initializer)
      : Stmt(StmtKind::Binding, std::move(location)), mutableBinding(mutableBinding),
        name(std::move(name)), initializer(std::move(initializer)) {}
  bool mutableBinding;
  std::string name;
  std::unique_ptr<Expr> initializer;
};

struct AssignmentStmt final : Stmt {
  AssignmentStmt(Location location, std::string name, std::unique_ptr<Expr> value)
      : Stmt(StmtKind::Assignment, std::move(location)), name(std::move(name)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Expr> value;
};

struct ReturnStmt final : Stmt {
  ReturnStmt(Location location, std::unique_ptr<Expr> value)
      : Stmt(StmtKind::Return, std::move(location)), value(std::move(value)) {}
  std::unique_ptr<Expr> value;
};

struct ExprStmt final : Stmt {
  ExprStmt(Location location, std::unique_ptr<Expr> expression)
      : Stmt(StmtKind::Expression, std::move(location)), expression(std::move(expression)) {}
  std::unique_ptr<Expr> expression;
};

struct IfStmt final : Stmt {
  IfStmt(Location location, std::unique_ptr<Expr> condition,
         std::vector<std::unique_ptr<Stmt>> thenBody,
         std::vector<std::unique_ptr<Stmt>> elseBody)
      : Stmt(StmtKind::If, std::move(location)), condition(std::move(condition)),
        thenBody(std::move(thenBody)), elseBody(std::move(elseBody)) {}
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> thenBody;
  std::vector<std::unique_ptr<Stmt>> elseBody;
};

struct WhileStmt final : Stmt {
  WhileStmt(Location location, std::unique_ptr<Expr> condition,
            std::vector<std::unique_ptr<Stmt>> body)
      : Stmt(StmtKind::While, std::move(location)), condition(std::move(condition)),
        body(std::move(body)) {}
  std::unique_ptr<Expr> condition;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct ForStmt final : Stmt {
  ForStmt(Location location, std::string name, std::unique_ptr<Expr> start,
          std::unique_ptr<Expr> end, std::vector<std::unique_ptr<Stmt>> body)
      : Stmt(StmtKind::For, std::move(location)), name(std::move(name)), start(std::move(start)),
        end(std::move(end)), body(std::move(body)) {}
  std::string name;
  std::unique_ptr<Expr> start;
  std::unique_ptr<Expr> end;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct LoopControlStmt final : Stmt {
  LoopControlStmt(StmtKind kind, Location location) : Stmt(kind, std::move(location)) {}
};

struct Parameter { std::string name; std::string typeName; Location location; };

struct Function {
  std::string name;
  Location location;
  std::vector<Parameter> parameters;
  std::string returnType;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct Module { std::vector<Function> functions; };

} // namespace rocket
