#include "parser.h"

#include <utility>

namespace rocket {

const Token& Parser::current() const { return tokens_[index_]; }
const Token& Parser::previous() const { return tokens_[index_ - 1]; }
bool Parser::at(TokenKind kind) const { return current().kind == kind; }
bool Parser::match(TokenKind kind) { if (!at(kind)) return false; ++index_; return true; }
bool Parser::matchAny(std::initializer_list<TokenKind> kinds) {
  for (auto kind : kinds) if (match(kind)) return true;
  return false;
}

const Token& Parser::consume(TokenKind kind, const std::string& message) {
  if (at(kind)) return tokens_[index_++];
  diagnostics_.error(current().location, message + "; found " + tokenName(current().kind));
  if (!at(TokenKind::End)) return tokens_[index_++];
  return current();
}

void Parser::skipNewlines() { while (match(TokenKind::Newline)) {} }

void Parser::synchronize() {
  while (!at(TokenKind::End) && !at(TokenKind::Newline) && !at(TokenKind::Dedent)) ++index_;
  skipNewlines();
}

Module Parser::parseModule() {
  Module module;
  skipNewlines();
  while (!at(TokenKind::End)) {
    if (!at(TokenKind::KwFn)) {
      diagnostics_.error(current().location, "top-level declarations must begin with 'fn'");
      synchronize();
      continue;
    }
    module.functions.push_back(parseFunction());
    skipNewlines();
  }
  return module;
}

Function Parser::parseFunction() {
  const Token start = consume(TokenKind::KwFn, "expected 'fn'");
  const Token name = consume(TokenKind::Identifier, "expected function name");
  consume(TokenKind::LParen, "expected '(' after function name");
  std::vector<Parameter> parameters;
  if (!at(TokenKind::RParen)) {
    do {
      const Token param = consume(TokenKind::Identifier, "expected parameter name");
      consume(TokenKind::Colon, "expected ':' after parameter name");
      const Token type = consume(TokenKind::Identifier, "expected parameter type");
      parameters.push_back({param.text, type.text, param.location});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::RParen, "expected ')' after parameters");
  consume(TokenKind::Arrow, "expected '->' and an explicit return type");
  const Token returnType = consume(TokenKind::Identifier, "expected return type");
  consume(TokenKind::Colon, "expected ':' before function body");
  consume(TokenKind::Newline, "expected newline after function signature");
  auto body = parseBlock();
  return {name.text, start.location, std::move(parameters), returnType.text, std::move(body)};
}

std::vector<std::unique_ptr<Stmt>> Parser::parseBlock() {
  consume(TokenKind::Indent, "expected an indented block");
  std::vector<std::unique_ptr<Stmt>> body;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    const std::size_t errorCount = diagnostics_.size();
    body.push_back(parseStatement());
    if (diagnostics_.size() != errorCount) synchronize();
  }
  consume(TokenKind::Dedent, "expected end of indented block");
  return body;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
  if (matchAny({TokenKind::KwLet, TokenKind::KwVar})) {
    const Token keyword = previous();
    const bool isMutable = keyword.kind == TokenKind::KwVar;
    const Token name = consume(TokenKind::Identifier, "expected binding name");
    consume(TokenKind::Equal, "expected '=' after binding name");
    auto value = parseExpression();
    consume(TokenKind::Newline, "expected newline after binding");
    return std::make_unique<BindingStmt>(keyword.location, isMutable, name.text, std::move(value));
  }
  if (match(TokenKind::KwReturn)) {
    const Token keyword = previous();
    std::unique_ptr<Expr> value;
    if (!at(TokenKind::Newline)) value = parseExpression();
    consume(TokenKind::Newline, "expected newline after return");
    return std::make_unique<ReturnStmt>(keyword.location, std::move(value));
  }
  if (at(TokenKind::KwIf)) return parseIf();
  if (at(TokenKind::KwWhile)) return parseWhile();
  if (at(TokenKind::KwFor)) return parseFor();
  if (match(TokenKind::KwBreak)) {
    const Token keyword = previous();
    consume(TokenKind::Newline, "expected newline after 'break'");
    return std::make_unique<LoopControlStmt>(StmtKind::Break, keyword.location);
  }
  if (match(TokenKind::KwContinue)) {
    const Token keyword = previous();
    consume(TokenKind::Newline, "expected newline after 'continue'");
    return std::make_unique<LoopControlStmt>(StmtKind::Continue, keyword.location);
  }
  if (at(TokenKind::Identifier) && index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind == TokenKind::Equal) {
    const Token name = consume(TokenKind::Identifier, "expected assignment target");
    consume(TokenKind::Equal, "expected '=' after assignment target");
    auto value = parseExpression();
    consume(TokenKind::Newline, "expected newline after assignment");
    return std::make_unique<AssignmentStmt>(name.location, name.text, std::move(value));
  }
  const Location location = current().location;
  auto expression = parseExpression();
  consume(TokenKind::Newline, "expected newline after expression");
  return std::make_unique<ExprStmt>(location, std::move(expression));
}

std::unique_ptr<Stmt> Parser::parseIf() {
  const Token keyword = consume(TokenKind::KwIf, "expected 'if'");
  auto condition = parseExpression();
  consume(TokenKind::Colon, "expected ':' after condition");
  consume(TokenKind::Newline, "expected newline after condition");
  auto thenBody = parseBlock();
  std::vector<std::unique_ptr<Stmt>> elseBody;
  if (match(TokenKind::KwElse)) {
    consume(TokenKind::Colon, "expected ':' after 'else'");
    consume(TokenKind::Newline, "expected newline after 'else'");
    elseBody = parseBlock();
  }
  return std::make_unique<IfStmt>(keyword.location, std::move(condition),
                                  std::move(thenBody), std::move(elseBody));
}

std::unique_ptr<Stmt> Parser::parseWhile() {
  const Token keyword = consume(TokenKind::KwWhile, "expected 'while'");
  auto condition = parseExpression();
  consume(TokenKind::Colon, "expected ':' after condition");
  consume(TokenKind::Newline, "expected newline after condition");
  return std::make_unique<WhileStmt>(keyword.location, std::move(condition), parseBlock());
}

std::unique_ptr<Stmt> Parser::parseFor() {
  const Token keyword = consume(TokenKind::KwFor, "expected 'for'");
  const Token name = consume(TokenKind::Identifier, "expected loop variable name");
  consume(TokenKind::KwIn, "expected 'in' after loop variable");
  auto start = parseExpression();
  consume(TokenKind::DotDot, "expected '..' between range bounds");
  auto end = parseExpression();
  consume(TokenKind::Colon, "expected ':' after range");
  consume(TokenKind::Newline, "expected newline after range");
  return std::make_unique<ForStmt>(keyword.location, name.text, std::move(start), std::move(end), parseBlock());
}

std::unique_ptr<Expr> Parser::parseExpression() { return parseOr(); }

std::unique_ptr<Expr> Parser::parseOr() {
  auto expression = parseAnd();
  while (match(TokenKind::KwOr)) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind, parseAnd());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseAnd() {
  auto expression = parseEquality();
  while (match(TokenKind::KwAnd)) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind, parseEquality());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseEquality() {
  auto expression = parseComparison();
  while (matchAny({TokenKind::EqualEqual, TokenKind::BangEqual})) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind,
                                               parseComparison());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseComparison() {
  auto expression = parseTerm();
  while (matchAny({TokenKind::Less, TokenKind::LessEqual, TokenKind::Greater,
                   TokenKind::GreaterEqual})) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind, parseTerm());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseTerm() {
  auto expression = parseFactor();
  while (matchAny({TokenKind::Plus, TokenKind::Minus})) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind, parseFactor());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseFactor() {
  auto expression = parseUnary();
  while (matchAny({TokenKind::Star, TokenKind::Slash})) {
    const Token op = previous();
    expression = std::make_unique<BinaryExpr>(op.location, std::move(expression), op.kind, parseUnary());
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parseUnary() {
  if (matchAny({TokenKind::Minus, TokenKind::KwNot})) {
    const Token op = previous();
    return std::make_unique<UnaryExpr>(op.location, op.kind, parseUnary());
  }
  return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
  auto expression = parsePrimary();
  while (match(TokenKind::LParen)) {
    const Location location = previous().location;
    std::vector<std::unique_ptr<Expr>> arguments;
    if (!at(TokenKind::RParen)) {
      do { arguments.push_back(parseExpression()); } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "expected ')' after arguments");
    expression = std::make_unique<CallExpr>(location, std::move(expression), std::move(arguments));
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::Integer)) return std::make_unique<LiteralExpr>(ExprKind::Integer, previous().location, previous().text);
  if (match(TokenKind::Float)) return std::make_unique<LiteralExpr>(ExprKind::Float, previous().location, previous().text);
  if (match(TokenKind::Character)) return std::make_unique<LiteralExpr>(ExprKind::Character, previous().location, previous().text);
  if (match(TokenKind::String)) return std::make_unique<LiteralExpr>(ExprKind::String, previous().location, previous().text);
  if (matchAny({TokenKind::KwTrue, TokenKind::KwFalse})) return std::make_unique<LiteralExpr>(ExprKind::Bool, previous().location, previous().text);
  if (match(TokenKind::Identifier)) return std::make_unique<LiteralExpr>(ExprKind::Name, previous().location, previous().text);
  if (match(TokenKind::LParen)) {
    auto expression = parseExpression();
    consume(TokenKind::RParen, "expected ')' after expression");
    return expression;
  }
  const Token unexpected = current();
  diagnostics_.error(unexpected.location, "expected expression; found " + std::string(tokenName(unexpected.kind)));
  if (!at(TokenKind::End)) ++index_;
  return std::make_unique<LiteralExpr>(ExprKind::Integer, unexpected.location, "0");
}

} // namespace rocket
