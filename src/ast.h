#pragma once

#include "token.h"

#include <memory>
#include <string>
#include <vector>

namespace rocket {

enum class ExprKind { Integer, String, Bool, Name, Unary, Binary, Call };

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

enum class StmtKind { Binding, Return, Expression, If, While };

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
