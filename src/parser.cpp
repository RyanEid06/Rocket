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
  diagnostics_.error(current().location, message + "; found " + tokenName(current().kind),
                     DiagnosticCode::Syntax);
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
    if (match(TokenKind::KwImport)) {
      const Token start = previous();
      const Token first = consume(TokenKind::Identifier, "expected module name after 'import'");
      std::string name = first.text;
      while (match(TokenKind::Dot)) {
        name += "." + consume(TokenKind::Identifier, "expected module name after '.'").text;
      }
      consume(TokenKind::Newline, "expected newline after import");
      module.imports.push_back({std::move(name), start.location});
      skipNewlines();
      continue;
    }
    const bool isPublic = match(TokenKind::KwPub);
    if (at(TokenKind::KwFn)) {
      module.functions.push_back(parseFunction(isPublic));
    } else if (at(TokenKind::KwStruct)) {
      module.structs.push_back(parseStruct(isPublic));
    } else if (at(TokenKind::KwEnum)) {
      module.enums.push_back(parseEnum(isPublic));
    } else {
      diagnostics_.error(current().location,
                         "expected 'fn', 'struct', 'enum', or 'import' at top level",
                         DiagnosticCode::Syntax);
      synchronize();
      continue;
    }
    skipNewlines();
  }
  return module;
}

std::vector<std::string> Parser::parseTypeParameters() {
  std::vector<std::string> parameters;
  if (!match(TokenKind::LBracket)) return parameters;
  do {
    parameters.push_back(consume(TokenKind::Identifier, "expected type parameter").text);
  } while (match(TokenKind::Comma));
  consume(TokenKind::RBracket, "expected ']' after type parameters");
  return parameters;
}

Function Parser::parseFunction(bool isPublic) {
  const Token start = consume(TokenKind::KwFn, "expected 'fn'");
  const Token name = consume(TokenKind::Identifier, "expected function name");
  auto typeParameters = parseTypeParameters();
  consume(TokenKind::LParen, "expected '(' after function name");
  std::vector<Parameter> parameters;
  if (!at(TokenKind::RParen)) {
    do {
      const Token param = consume(TokenKind::Identifier, "expected parameter name");
      consume(TokenKind::Colon, "expected ':' after parameter name");
      const std::string type = parseTypeName();
      parameters.push_back({param.text, type, param.location});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::RParen, "expected ')' after parameters");
  consume(TokenKind::Arrow, "expected '->' and an explicit return type");
  const std::string returnType = parseTypeName();
  consume(TokenKind::Colon, "expected ':' before function body");
  consume(TokenKind::Newline, "expected newline after function signature");
  auto body = parseBlock();
  return {name.text, start.location, isPublic, std::move(typeParameters),
          std::move(parameters), returnType, std::move(body)};
}

StructDecl Parser::parseStruct(bool isPublic) {
  const Token start = consume(TokenKind::KwStruct, "expected 'struct'");
  const Token name = consume(TokenKind::Identifier, "expected struct name");
  auto typeParameters = parseTypeParameters();
  consume(TokenKind::Colon, "expected ':' after struct name");
  consume(TokenKind::Newline, "expected newline after struct declaration");
  consume(TokenKind::Indent, "expected an indented struct body");
  std::vector<TypeField> fields;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    const Token field = consume(TokenKind::Identifier, "expected field name");
    consume(TokenKind::Colon, "expected ':' after field name");
    fields.push_back({field.text, parseTypeName(), field.location});
    consume(TokenKind::Newline, "expected newline after field");
  }
  consume(TokenKind::Dedent, "expected end of struct body");
  return {name.text, start.location, isPublic, std::move(typeParameters), std::move(fields)};
}

EnumDecl Parser::parseEnum(bool isPublic) {
  const Token start = consume(TokenKind::KwEnum, "expected 'enum'");
  const Token name = consume(TokenKind::Identifier, "expected enum name");
  auto typeParameters = parseTypeParameters();
  consume(TokenKind::Colon, "expected ':' after enum name");
  consume(TokenKind::Newline, "expected newline after enum declaration");
  consume(TokenKind::Indent, "expected an indented enum body");
  std::vector<EnumVariant> variants;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    const Token variant = consume(TokenKind::Identifier, "expected variant name");
    std::vector<std::string> payloadTypes;
    if (match(TokenKind::LParen)) {
      if (!at(TokenKind::RParen)) {
        do { payloadTypes.push_back(parseTypeName()); } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RParen, "expected ')' after variant payload types");
    }
    consume(TokenKind::Newline, "expected newline after enum variant");
    variants.push_back({variant.text, variant.location, std::move(payloadTypes)});
  }
  consume(TokenKind::Dedent, "expected end of enum body");
  return {name.text, start.location, isPublic, std::move(typeParameters), std::move(variants)};
}

std::string Parser::parseTypeName() {
  const Token outer = consume(TokenKind::Identifier, "expected type");
  std::string result = outer.text;
  while (match(TokenKind::Dot))
    result += "." + consume(TokenKind::Identifier, "expected type name after '.'").text;
  if (!match(TokenKind::LBracket)) return result;
  result += '[';
  bool first = true;
  do {
    if (!first) result += ", ";
    result += parseTypeName();
    first = false;
  } while (match(TokenKind::Comma));
  consume(TokenKind::RBracket, "expected ']' after type arguments");
  result += ']';
  return result;
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
    std::string declaredType;
    if (match(TokenKind::Colon)) declaredType = parseTypeName();
    consume(TokenKind::Equal, "expected '=' after binding name");
    auto value = parseExpression();
    consume(TokenKind::Newline, "expected newline after binding");
    return std::make_unique<BindingStmt>(keyword.location, isMutable, name.text,
                                         std::move(declaredType), std::move(value));
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
  if (at(TokenKind::KwMatch)) return parseMatch();
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
  const Location location = current().location;
  auto expression = parseExpression();
  if (match(TokenKind::Equal)) {
    auto value = parseExpression();
    consume(TokenKind::Newline, "expected newline after assignment");
    if (expression->kind == ExprKind::Name) {
      auto& name = static_cast<LiteralExpr&>(*expression);
      return std::make_unique<AssignmentStmt>(location, name.value, std::move(value));
    }
    if (expression->kind == ExprKind::Index) {
      auto& indexed = static_cast<IndexExpr&>(*expression);
      if (indexed.collection->kind == ExprKind::Name) {
        auto& name = static_cast<LiteralExpr&>(*indexed.collection);
        return std::make_unique<IndexAssignmentStmt>(
            location, name.value, std::move(indexed.index), std::move(value));
      }
    }
    diagnostics_.error(location,
                       "assignment target must be a binding or Array element",
                       DiagnosticCode::Syntax);
    return std::make_unique<ExprStmt>(location, std::move(value));
  }
  consume(TokenKind::Newline, "expected newline after expression");
  return std::make_unique<ExprStmt>(location, std::move(expression));
}

std::unique_ptr<Stmt> Parser::parseMatch() {
  const Token keyword = consume(TokenKind::KwMatch, "expected 'match'");
  auto value = parseExpression();
  consume(TokenKind::Colon, "expected ':' after match value");
  consume(TokenKind::Newline, "expected newline after match value");
  consume(TokenKind::Indent, "expected an indented match body");
  std::vector<MatchCase> cases;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    consume(TokenKind::KwCase, "expected 'case' in match body");
    const Token variant = consume(TokenKind::Identifier, "expected variant name or '_'");
    std::string variantName = variant.text;
    while (match(TokenKind::Dot))
      variantName += "." + consume(TokenKind::Identifier,
                                    "expected variant name after '.'").text;
    MatchPattern pattern{variant.location, variantName, variantName == "_", {}};
    if (match(TokenKind::LParen)) {
      if (!at(TokenKind::RParen)) {
        do {
          pattern.bindings.push_back(
              consume(TokenKind::Identifier, "expected pattern binding").text);
        } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RParen, "expected ')' after pattern bindings");
    }
    consume(TokenKind::Colon, "expected ':' after pattern");
    consume(TokenKind::Newline, "expected newline after pattern");
    cases.push_back({std::move(pattern), parseBlock()});
  }
  consume(TokenKind::Dedent, "expected end of match body");
  return std::make_unique<MatchStmt>(keyword.location, std::move(value), std::move(cases));
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
  while (true) {
    if (match(TokenKind::LParen)) {
      const Location location = previous().location;
      std::vector<std::unique_ptr<Expr>> arguments;
      if (!at(TokenKind::RParen)) {
        do { arguments.push_back(parseExpression()); } while (match(TokenKind::Comma));
      }
      consume(TokenKind::RParen, "expected ')' after arguments");
      expression = std::make_unique<CallExpr>(location, std::move(expression),
                                              std::move(arguments));
      continue;
    }
    if (match(TokenKind::LBracket)) {
      const Location location = previous().location;
      auto first = parseExpression();
      if (match(TokenKind::DotDot)) {
        auto end = parseExpression();
        consume(TokenKind::RBracket, "expected ']' after slice bounds");
        expression = std::make_unique<SliceExpr>(location, std::move(expression),
                                                 std::move(first), std::move(end));
      } else {
        consume(TokenKind::RBracket, "expected ']' after index");
        expression = std::make_unique<IndexExpr>(location, std::move(expression),
                                                 std::move(first));
      }
      continue;
    }
    if (match(TokenKind::Dot)) {
      const Token field = consume(TokenKind::Identifier, "expected field name after '.'");
      expression = std::make_unique<FieldExpr>(field.location, std::move(expression),
                                               field.text);
      continue;
    }
    if (match(TokenKind::Question)) {
      expression = std::make_unique<PropagateExpr>(previous().location,
                                                   std::move(expression));
      continue;
    }
    break;
  }
  return expression;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
  if (match(TokenKind::LBracket)) {
    const Location location = previous().location;
    std::vector<std::unique_ptr<Expr>> elements;
    if (!at(TokenKind::RBracket)) {
      do { elements.push_back(parseExpression()); } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RBracket, "expected ']' after Array literal");
    return std::make_unique<ArrayExpr>(location, std::move(elements));
  }
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
  diagnostics_.error(unexpected.location,
                     "expected expression; found " + std::string(tokenName(unexpected.kind)),
                     DiagnosticCode::Syntax);
  if (!at(TokenKind::End)) ++index_;
  return std::make_unique<LiteralExpr>(ExprKind::Integer, unexpected.location, "0");
}

} // namespace rocket
