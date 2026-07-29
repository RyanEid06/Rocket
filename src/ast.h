#pragma once

#include "token.h"

#include <memory>
#include <string>
#include <vector>

namespace rocket {

struct Parameter { std::string name; std::string typeName; Location location; };

enum class ExprKind {
  Integer, Float, Character, String, Bool, Name, Unary, Binary, Call, Array, Index, Slice,
  Field, Propagate, Lambda
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

struct FieldExpr final : Expr {
  FieldExpr(Location location, std::unique_ptr<Expr> value, std::string field)
      : Expr(ExprKind::Field, std::move(location)), value(std::move(value)),
        field(std::move(field)) {}
  std::unique_ptr<Expr> value;
  std::string field;
};

struct PropagateExpr final : Expr {
  PropagateExpr(Location location, std::unique_ptr<Expr> value)
      : Expr(ExprKind::Propagate, std::move(location)), value(std::move(value)) {}
  std::unique_ptr<Expr> value;
};

enum class StmtKind {
  Binding, Assignment, IndexAssignment, Return, Expression, If, While, For, Break,
  Continue, Match
};

struct Stmt {
  explicit Stmt(StmtKind kind, Location location) : kind(kind), location(std::move(location)) {}
  virtual ~Stmt() = default;
  StmtKind kind;
  Location location;
};

struct BindingStmt final : Stmt {
  BindingStmt(Location location, bool mutableBinding, std::string name,
              std::string declaredType, std::unique_ptr<Expr> initializer)
      : Stmt(StmtKind::Binding, std::move(location)), mutableBinding(mutableBinding),
        name(std::move(name)), declaredType(std::move(declaredType)),
        initializer(std::move(initializer)) {}
  bool mutableBinding;
  std::string name;
  std::string declaredType;
  std::unique_ptr<Expr> initializer;
};

struct AssignmentStmt final : Stmt {
  AssignmentStmt(Location location, std::string name, std::unique_ptr<Expr> value)
      : Stmt(StmtKind::Assignment, std::move(location)), name(std::move(name)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Expr> value;
};

struct IndexAssignmentStmt final : Stmt {
  IndexAssignmentStmt(Location location, std::string name, std::unique_ptr<Expr> index,
                      std::unique_ptr<Expr> value)
      : Stmt(StmtKind::IndexAssignment, std::move(location)), name(std::move(name)),
        index(std::move(index)), value(std::move(value)) {}
  std::string name;
  std::unique_ptr<Expr> index;
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
          std::unique_ptr<Expr> end, std::vector<std::unique_ptr<Stmt>> body,
          bool rangeLoop = true)
      : Stmt(StmtKind::For, std::move(location)), name(std::move(name)), start(std::move(start)),
        end(std::move(end)), body(std::move(body)), rangeLoop(rangeLoop) {}
  std::string name;
  std::unique_ptr<Expr> start;
  std::unique_ptr<Expr> end;
  std::vector<std::unique_ptr<Stmt>> body;
  bool rangeLoop = true;
};

struct LoopControlStmt final : Stmt {
  LoopControlStmt(StmtKind kind, Location location) : Stmt(kind, std::move(location)) {}
};

struct MatchPattern {
  Location location;
  std::string variant;
  bool wildcard = false;
  std::vector<std::string> bindings;
};

struct MatchCase {
  MatchPattern pattern;
  std::vector<std::unique_ptr<Stmt>> body;
};

struct MatchStmt final : Stmt {
  MatchStmt(Location location, std::unique_ptr<Expr> value,
            std::vector<MatchCase> cases)
      : Stmt(StmtKind::Match, std::move(location)), value(std::move(value)),
        cases(std::move(cases)) {}
  std::unique_ptr<Expr> value;
  std::vector<MatchCase> cases;
};

struct TraitConstraint {
  std::string typeParameter;
  std::string traitName;
  Location location;
};

struct LambdaExpr final : Expr {
  LambdaExpr(Location location, std::vector<Parameter> parameters,
             std::string returnType, std::unique_ptr<Expr> body)
      : Expr(ExprKind::Lambda, std::move(location)), parameters(std::move(parameters)),
        returnType(std::move(returnType)), body(std::move(body)) {}
  std::vector<Parameter> parameters;
  std::string returnType;
  std::unique_ptr<Expr> body;
};

struct Function {
  std::string name;
  Location location;
  bool publicDeclaration = false;
  std::vector<std::string> typeParameters;
  std::vector<Parameter> parameters;
  std::string returnType;
  std::vector<std::unique_ptr<Stmt>> body;
  // Non-empty for functions declared in an impl block. The owner spelling may
  // contain the impl's type parameters; name remains the short member name
  // until module loading assigns its deterministic qualified identity.
  std::string methodOwner;
  // Non-empty for a method that satisfies a trait implementation.
  std::string methodTrait;
  std::vector<TraitConstraint> constraints;
  bool associatedConstant = false;
};

struct TraitMethod {
  std::string name;
  Location location;
  std::vector<Parameter> parameters;
  std::string returnType;
};

struct TraitDecl {
  std::string name;
  Location location;
  bool publicDeclaration = false;
  std::vector<TraitMethod> methods;
};

struct TypeField { std::string name; std::string typeName; Location location; };

struct StructDecl {
  std::string name;
  Location location;
  bool publicDeclaration = false;
  std::vector<std::string> typeParameters;
  std::vector<TypeField> fields;
};

struct EnumVariant {
  std::string name;
  Location location;
  std::vector<std::string> payloadTypes;
};

struct EnumDecl {
  std::string name;
  Location location;
  bool publicDeclaration = false;
  std::vector<std::string> typeParameters;
  std::vector<EnumVariant> variants;
};

struct ImportDecl { std::string name; Location location; };

struct Module {
  std::string name;
  std::vector<ImportDecl> imports;
  std::vector<StructDecl> structs;
  std::vector<EnumDecl> enums;
  std::vector<TraitDecl> traits;
  std::vector<Function> functions;
};

} // namespace rocket
