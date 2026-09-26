#include "ast_clone.h"
#include "analysis_control.h"

namespace rocket {
namespace {

std::unique_ptr<Expr> cloneExpr(const Expr *source);
std::unique_ptr<Stmt> cloneStmt(const Stmt *source);

template <typename T, typename Clone>
std::vector<std::unique_ptr<T>>
clonePointers(const std::vector<std::unique_ptr<T>> &source, Clone clone) {
  std::vector<std::unique_ptr<T>> result;
  result.reserve(source.size());
  for (const auto &item : source) {
    analysisCheckpoint();
    result.push_back(clone(item.get()));
  }
  return result;
}

Parameter cloneParameter(const Parameter &source) {
  Parameter result;
  result.name = source.name;
  result.typeName = source.typeName;
  result.location = source.location;
  result.defaultValue = cloneExpr(source.defaultValue.get());
  result.defaultText = source.defaultText;
  return result;
}

std::vector<Parameter> cloneParameters(const std::vector<Parameter> &source) {
  std::vector<Parameter> result;
  result.reserve(source.size());
  for (const auto &item : source)
    result.push_back(cloneParameter(item));
  return result;
}

std::unique_ptr<Expr> cloneExpr(const Expr *source) {
  analysisCheckpoint();
  if (!source)
    return {};
  switch (source->kind) {
  case ExprKind::Integer:
  case ExprKind::Float:
  case ExprKind::Character:
  case ExprKind::String:
  case ExprKind::Bool:
  case ExprKind::Name: {
    const auto &item = static_cast<const LiteralExpr &>(*source);
    return std::make_unique<LiteralExpr>(item.kind, item.location, item.value);
  }
  case ExprKind::Unary: {
    const auto &item = static_cast<const UnaryExpr &>(*source);
    return std::make_unique<UnaryExpr>(item.location, item.op,
                                       cloneExpr(item.operand.get()));
  }
  case ExprKind::Binary: {
    const auto &item = static_cast<const BinaryExpr &>(*source);
    return std::make_unique<BinaryExpr>(item.location,
                                        cloneExpr(item.left.get()), item.op,
                                        cloneExpr(item.right.get()));
  }
  case ExprKind::Call: {
    const auto &item = static_cast<const CallExpr &>(*source);
    return std::make_unique<CallExpr>(item.location,
                                      cloneExpr(item.callee.get()),
                                      clonePointers(item.arguments, cloneExpr));
  }
  case ExprKind::NamedArgument: {
    const auto &item = static_cast<const NamedArgumentExpr &>(*source);
    return std::make_unique<NamedArgumentExpr>(item.location, item.name,
                                               cloneExpr(item.value.get()));
  }
  case ExprKind::Array: {
    const auto &item = static_cast<const ArrayExpr &>(*source);
    return std::make_unique<ArrayExpr>(item.location,
                                       clonePointers(item.elements, cloneExpr));
  }
  case ExprKind::Index: {
    const auto &item = static_cast<const IndexExpr &>(*source);
    return std::make_unique<IndexExpr>(item.location,
                                       cloneExpr(item.collection.get()),
                                       cloneExpr(item.index.get()));
  }
  case ExprKind::Slice: {
    const auto &item = static_cast<const SliceExpr &>(*source);
    return std::make_unique<SliceExpr>(
        item.location, cloneExpr(item.collection.get()),
        cloneExpr(item.start.get()), cloneExpr(item.end.get()));
  }
  case ExprKind::Field: {
    const auto &item = static_cast<const FieldExpr &>(*source);
    return std::make_unique<FieldExpr>(item.location,
                                       cloneExpr(item.value.get()), item.field);
  }
  case ExprKind::Propagate: {
    const auto &item = static_cast<const PropagateExpr &>(*source);
    return std::make_unique<PropagateExpr>(item.location,
                                           cloneExpr(item.value.get()));
  }
  case ExprKind::Await: {
    const auto &item = static_cast<const AwaitExpr &>(*source);
    return std::make_unique<AwaitExpr>(item.location,
                                       cloneExpr(item.value.get()));
  }
  case ExprKind::Lambda: {
    const auto &item = static_cast<const LambdaExpr &>(*source);
    return std::make_unique<LambdaExpr>(
        item.location, cloneParameters(item.parameters), item.returnType,
        cloneExpr(item.body.get()));
  }
  }
  return {};
}

std::unique_ptr<Stmt> cloneStmt(const Stmt *source) {
  analysisCheckpoint();
  if (!source)
    return {};
  switch (source->kind) {
  case StmtKind::Binding: {
    const auto &item = static_cast<const BindingStmt &>(*source);
    return std::make_unique<BindingStmt>(item.location, item.mutableBinding,
                                         item.name, item.declaredType,
                                         cloneExpr(item.initializer.get()));
  }
  case StmtKind::Assignment: {
    const auto &item = static_cast<const AssignmentStmt &>(*source);
    return std::make_unique<AssignmentStmt>(item.location, item.name,
                                            cloneExpr(item.value.get()));
  }
  case StmtKind::IndexAssignment: {
    const auto &item = static_cast<const IndexAssignmentStmt &>(*source);
    return std::make_unique<IndexAssignmentStmt>(item.location, item.name,
                                                 cloneExpr(item.index.get()),
                                                 cloneExpr(item.value.get()));
  }
  case StmtKind::Return: {
    const auto &item = static_cast<const ReturnStmt &>(*source);
    return std::make_unique<ReturnStmt>(item.location,
                                        cloneExpr(item.value.get()));
  }
  case StmtKind::Expression: {
    const auto &item = static_cast<const ExprStmt &>(*source);
    return std::make_unique<ExprStmt>(item.location,
                                      cloneExpr(item.expression.get()));
  }
  case StmtKind::If: {
    const auto &item = static_cast<const IfStmt &>(*source);
    return std::make_unique<IfStmt>(item.location,
                                    cloneExpr(item.condition.get()),
                                    clonePointers(item.thenBody, cloneStmt),
                                    clonePointers(item.elseBody, cloneStmt));
  }
  case StmtKind::While: {
    const auto &item = static_cast<const WhileStmt &>(*source);
    return std::make_unique<WhileStmt>(item.location,
                                       cloneExpr(item.condition.get()),
                                       clonePointers(item.body, cloneStmt));
  }
  case StmtKind::For: {
    const auto &item = static_cast<const ForStmt &>(*source);
    return std::make_unique<ForStmt>(
        item.location, item.name, cloneExpr(item.start.get()),
        cloneExpr(item.end.get()), clonePointers(item.body, cloneStmt),
        item.rangeLoop);
  }
  case StmtKind::Break:
  case StmtKind::Continue:
    return std::make_unique<LoopControlStmt>(source->kind, source->location);
  case StmtKind::Match: {
    const auto &item = static_cast<const MatchStmt &>(*source);
    std::vector<MatchCase> cases;
    cases.reserve(item.cases.size());
    for (const auto &matchCase : item.cases)
      cases.push_back(
          {matchCase.pattern, clonePointers(matchCase.body, cloneStmt)});
    return std::make_unique<MatchStmt>(
        item.location, cloneExpr(item.value.get()), std::move(cases));
  }
  case StmtKind::Unsafe: {
    const auto &item = static_cast<const UnsafeStmt &>(*source);
    return std::make_unique<UnsafeStmt>(item.location,
                                        clonePointers(item.body, cloneStmt));
  }
  }
  return {};
}

Function cloneFunction(const Function &source) {
  Function result;
  result.name = source.name;
  result.location = source.location;
  result.publicDeclaration = source.publicDeclaration;
  result.typeParameters = source.typeParameters;
  result.parameters = cloneParameters(source.parameters);
  result.returnType = source.returnType;
  result.body = clonePointers(source.body, cloneStmt);
  result.methodOwner = source.methodOwner;
  result.methodTrait = source.methodTrait;
  result.constraints = source.constraints;
  result.associatedConstant = source.associatedConstant;
  result.nativeImport = source.nativeImport;
  result.nativeExport = source.nativeExport;
  result.nativeConstant = source.nativeConstant;
  result.nativeName = source.nativeName;
  result.asynchronous = source.asynchronous;
  return result;
}

StructDecl cloneStruct(const StructDecl &source) {
  StructDecl result;
  result.name = source.name;
  result.location = source.location;
  result.publicDeclaration = source.publicDeclaration;
  result.typeParameters = source.typeParameters;
  result.fields = source.fields;
  result.representation = source.representation;
  result.callbackParameters = cloneParameters(source.callbackParameters);
  result.callbackReturnType = source.callbackReturnType;
  result.nativeName = source.nativeName;
  return result;
}

} // namespace

Module cloneModule(const Module &source) {
  analysisCheckpoint();
  Module result;
  result.name = source.name;
  result.imports = source.imports;
  result.structs.reserve(source.structs.size());
  for (const auto &item : source.structs)
    result.structs.push_back(cloneStruct(item));
  result.enums = source.enums;
  result.traits.reserve(source.traits.size());
  for (const auto &item : source.traits) {
    TraitDecl copy;
    copy.name = item.name;
    copy.location = item.location;
    copy.publicDeclaration = item.publicDeclaration;
    for (const auto &method : item.methods)
      copy.methods.push_back({method.name, method.location,
                              cloneParameters(method.parameters),
                              method.returnType});
    result.traits.push_back(std::move(copy));
  }
  result.functions.reserve(source.functions.size());
  for (const auto &item : source.functions)
    result.functions.push_back(cloneFunction(item));
  result.library = source.library;
  return result;
}

} // namespace rocket
