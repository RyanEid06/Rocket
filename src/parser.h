#pragma once

#include "ast.h"
#include "diagnostic.h"

#include <initializer_list>
#include <vector>

namespace rocket {

class Parser {
public:
  Parser(const std::vector<Token>& tokens, Diagnostics& diagnostics)
      : tokens_(tokens), diagnostics_(diagnostics) {}
  Module parseModule();

private:
  const Token& current() const;
  const Token& previous() const;
  bool at(TokenKind kind) const;
  bool match(TokenKind kind);
  bool matchAny(std::initializer_list<TokenKind> kinds);
  const Token& consume(TokenKind kind, const std::string& message);
  void skipNewlines();
  void synchronize();

  Function parseFunction(bool isPublic);
  Function parseExternFunction(bool isPublic);
  Function parseExternConstant(bool isPublic);
  StructDecl parseExternType(bool isPublic);
  std::vector<Function> parseImpl();
  TraitDecl parseTrait(bool isPublic);
  TraitMethod parseTraitMethod();
  StructDecl parseStruct(bool isPublic);
  EnumDecl parseEnum(bool isPublic);
  std::vector<std::string> parseTypeParameters();
  std::vector<std::unique_ptr<Stmt>> parseBlock();
  std::unique_ptr<Stmt> parseStatement();
  std::unique_ptr<Stmt> parseIf();
  std::unique_ptr<Stmt> parseWhile();
  std::unique_ptr<Stmt> parseFor();
  std::unique_ptr<Stmt> parseMatch();
  std::unique_ptr<Stmt> parseUnsafe();
  std::unique_ptr<Expr> parseExpression();
  std::unique_ptr<Expr> parseOr();
  std::unique_ptr<Expr> parseAnd();
  std::unique_ptr<Expr> parseEquality();
  std::unique_ptr<Expr> parseComparison();
  std::unique_ptr<Expr> parseTerm();
  std::unique_ptr<Expr> parseFactor();
  std::unique_ptr<Expr> parseUnary();
  std::unique_ptr<Expr> parseCall();
  std::unique_ptr<Expr> parsePrimary();
  std::string parseTypeName();

  const std::vector<Token>& tokens_;
  Diagnostics& diagnostics_;
  std::size_t index_ = 0;
};

} // namespace rocket
