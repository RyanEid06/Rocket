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
    if (at(TokenKind::Identifier) && current().text == "extern") {
      ++index_;
      if (at(TokenKind::KwFn)) {
        module.functions.push_back(parseExternFunction(isPublic));
      } else if (at(TokenKind::KwConst)) {
        module.functions.push_back(parseExternConstant(isPublic));
      } else if (at(TokenKind::KwStruct) ||
                 (at(TokenKind::Identifier) &&
                  (current().text == "opaque" || current().text == "callback"))) {
        module.structs.push_back(parseExternType(isPublic));
      } else {
        diagnostics_.error(current().location,
                           "expected 'fn', 'const', 'struct', 'opaque', or 'callback' after 'extern'",
                           DiagnosticCode::Syntax);
        synchronize();
      }
    } else if (at(TokenKind::Identifier) && current().text == "export") {
      ++index_;
      if (!at(TokenKind::KwFn)) {
        diagnostics_.error(current().location, "expected 'fn' after 'export'",
                           DiagnosticCode::Syntax);
        synchronize();
      } else {
        Function function = parseFunction(true);
        function.nativeExport = true;
        function.nativeName = function.name;
        module.functions.push_back(std::move(function));
      }
    } else if (at(TokenKind::KwFn)) {
      module.functions.push_back(parseFunction(isPublic));
    } else if (at(TokenKind::KwStruct)) {
      module.structs.push_back(parseStruct(isPublic));
    } else if (at(TokenKind::KwEnum)) {
      module.enums.push_back(parseEnum(isPublic));
    } else if (at(TokenKind::KwTrait)) {
      module.traits.push_back(parseTrait(isPublic));
    } else if (at(TokenKind::KwImpl)) {
      if (isPublic)
        diagnostics_.error(current().location,
                           "an impl block is not public; mark individual methods 'pub'");
      auto methods = parseImpl();
      for (auto& method : methods) module.functions.push_back(std::move(method));
    } else {
      diagnostics_.error(current().location,
                         "expected a declaration or import at top level",
                         DiagnosticCode::Syntax);
      synchronize();
      continue;
    }
    skipNewlines();
  }
  return module;
}

Function Parser::parseExternFunction(bool isPublic) {
  const Token start = consume(TokenKind::KwFn, "expected 'fn'");
  const Token name = consume(TokenKind::Identifier, "expected native function name");
  consume(TokenKind::LParen, "expected '(' after native function name");
  std::vector<Parameter> parameters;
  if (!at(TokenKind::RParen)) {
    do {
      const Token parameter = consume(TokenKind::Identifier, "expected parameter name");
      consume(TokenKind::Colon, "expected ':' after parameter name");
      parameters.push_back({parameter.text, parseTypeName(), parameter.location});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::RParen, "expected ')' after native parameters");
  consume(TokenKind::Arrow, "expected '->' and native result type");
  const std::string resultType = parseTypeName();
  consume(TokenKind::Newline, "expected newline after native function declaration");
  Function function{name.text, start.location, isPublic, {}, std::move(parameters),
                    resultType, {}};
  function.nativeImport = true;
  function.nativeName = name.text;
  return function;
}

Function Parser::parseExternConstant(bool isPublic) {
  const Token start = consume(TokenKind::KwConst, "expected 'const'");
  const Token name = consume(TokenKind::Identifier, "expected native constant name");
  consume(TokenKind::Colon, "expected ':' after native constant name");
  const std::string type = parseTypeName();
  consume(TokenKind::Equal, "expected '=' after native constant type");
  auto value = parseExpression();
  consume(TokenKind::Newline, "expected newline after native constant");
  Function constant{name.text, start.location, isPublic, {}, {}, type, {}};
  constant.body.push_back(std::make_unique<ReturnStmt>(start.location, std::move(value)));
  constant.nativeConstant = true;
  constant.nativeName = name.text;
  return constant;
}

StructDecl Parser::parseExternType(bool isPublic) {
  StructDecl declaration;
  declaration.publicDeclaration = isPublic;
  if (at(TokenKind::Identifier) && current().text == "opaque") {
    ++index_;
    const Token name = consume(TokenKind::Identifier, "expected opaque type name");
    consume(TokenKind::Newline, "expected newline after opaque declaration");
    declaration.name = name.text;
    declaration.nativeName = name.text;
    declaration.location = name.location;
    declaration.representation = StructRepresentation::Opaque;
    return declaration;
  }
  if (at(TokenKind::Identifier) && current().text == "callback") {
    ++index_;
    const Token name = consume(TokenKind::Identifier, "expected callback type name");
    declaration.name = name.text;
    declaration.nativeName = name.text;
    declaration.location = name.location;
    declaration.representation = StructRepresentation::Callback;
    consume(TokenKind::LParen, "expected '(' after callback type name");
    if (!at(TokenKind::RParen)) {
      do {
        const Token parameter = consume(TokenKind::Identifier, "expected callback parameter name");
        consume(TokenKind::Colon, "expected ':' after callback parameter name");
        declaration.callbackParameters.push_back(
            {parameter.text, parseTypeName(), parameter.location});
      } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "expected ')' after callback parameters");
    consume(TokenKind::Arrow, "expected '->' and callback result type");
    declaration.callbackReturnType = parseTypeName();
    consume(TokenKind::Newline, "expected newline after callback declaration");
    return declaration;
  }

  const Token start = consume(TokenKind::KwStruct, "expected 'struct'");
  const Token name = consume(TokenKind::Identifier, "expected native struct name");
  declaration.name = name.text;
  declaration.nativeName = name.text;
  declaration.location = start.location;
  declaration.representation = StructRepresentation::Native;
  consume(TokenKind::Colon, "expected ':' after native struct name");
  consume(TokenKind::Newline, "expected newline after native struct declaration");
  consume(TokenKind::Indent, "expected an indented native struct body");
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    const Token field = consume(TokenKind::Identifier, "expected native struct field name");
    consume(TokenKind::Colon, "expected ':' after native struct field name");
    declaration.fields.push_back({field.text, parseTypeName(), field.location});
    consume(TokenKind::Newline, "expected newline after native struct field");
  }
  consume(TokenKind::Dedent, "expected end of native struct body");
  return declaration;
}

std::vector<Function> Parser::parseImpl() {
  consume(TokenKind::KwImpl, "expected 'impl'");
  auto implParameters = parseTypeParameters();
  std::string trait;
  std::string owner = parseTypeName();
  if (match(TokenKind::KwFor)) {
    trait = std::move(owner);
    owner = parseTypeName();
  }
  consume(TokenKind::Colon, "expected ':' after impl type");
  consume(TokenKind::Newline, "expected newline after impl declaration");
  consume(TokenKind::Indent, "expected an indented impl body");
  std::vector<Function> methods;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    const bool isPublic = match(TokenKind::KwPub);
    if (match(TokenKind::KwConst)) {
      const Token name = consume(TokenKind::Identifier,
                                 "expected associated constant name");
      consume(TokenKind::Colon, "expected ':' after associated constant name");
      const std::string resultType = parseTypeName();
      consume(TokenKind::Equal, "expected '=' in associated constant declaration");
      auto value = parseExpression();
      consume(TokenKind::Newline, "expected newline after associated constant");
      Function constant;
      constant.name = owner.substr(0, owner.find('[')) + "." + name.text;
      constant.location = name.location;
      constant.publicDeclaration = isPublic;
      constant.typeParameters = implParameters;
      constant.returnType = resultType;
      constant.body.push_back(std::make_unique<ReturnStmt>(name.location,
                                                           std::move(value)));
      constant.methodOwner = owner;
      constant.methodTrait = trait;
      constant.associatedConstant = true;
      if (!implParameters.empty())
        diagnostics_.error(name.location,
                           "associated constants in generic impl blocks are not supported");
      if (!trait.empty())
        diagnostics_.error(name.location,
                           "trait impl blocks cannot declare associated constants");
      methods.push_back(std::move(constant));
      continue;
    }
    if (!at(TokenKind::KwFn)) {
      diagnostics_.error(current().location,
                         "expected a method or associated constant in impl body",
                         DiagnosticCode::Syntax);
      synchronize();
      continue;
    }
    Function method = parseFunction(isPublic);
    std::vector<std::string> parameters = implParameters;
    parameters.insert(parameters.end(), method.typeParameters.begin(),
                      method.typeParameters.end());
    method.typeParameters = std::move(parameters);
    method.methodOwner = owner;
    method.methodTrait = trait;
    const std::size_t arguments = owner.find('[');
    const std::string ownerName = owner.substr(0, arguments);
    method.name = ownerName + "." + (trait.empty() ? std::string() : trait + ".") + method.name;
    methods.push_back(std::move(method));
  }
  consume(TokenKind::Dedent, "expected end of impl body");
  return methods;
}

TraitMethod Parser::parseTraitMethod() {
  const Token start = consume(TokenKind::KwFn, "expected 'fn'");
  const Token name = consume(TokenKind::Identifier, "expected trait method name");
  consume(TokenKind::LParen, "expected '(' after trait method name");
  std::vector<Parameter> parameters;
  if (!at(TokenKind::RParen)) {
    do {
      const Token parameter = consume(TokenKind::Identifier, "expected parameter name");
      consume(TokenKind::Colon, "expected ':' after parameter name");
      parameters.push_back({parameter.text, parseTypeName(), parameter.location});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::RParen, "expected ')' after trait method parameters");
  consume(TokenKind::Arrow, "expected '->' and an explicit return type");
  std::string result = parseTypeName();
  consume(TokenKind::Newline, "expected newline after trait method signature");
  return {name.text, start.location, std::move(parameters), std::move(result)};
}

TraitDecl Parser::parseTrait(bool isPublic) {
  const Token start = consume(TokenKind::KwTrait, "expected 'trait'");
  const Token name = consume(TokenKind::Identifier, "expected trait name");
  consume(TokenKind::Colon, "expected ':' after trait name");
  consume(TokenKind::Newline, "expected newline after trait declaration");
  consume(TokenKind::Indent, "expected an indented trait body");
  std::vector<TraitMethod> methods;
  while (!at(TokenKind::Dedent) && !at(TokenKind::End)) {
    if (match(TokenKind::Newline)) continue;
    if (match(TokenKind::KwPub))
      diagnostics_.error(previous().location,
                         "trait methods inherit the trait visibility and cannot be 'pub'");
    if (!at(TokenKind::KwFn)) {
      diagnostics_.error(current().location, "expected a method signature in trait body",
                         DiagnosticCode::Syntax);
      synchronize();
      continue;
    }
    methods.push_back(parseTraitMethod());
  }
  consume(TokenKind::Dedent, "expected end of trait body");
  return {name.text, start.location, isPublic, std::move(methods)};
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
  std::vector<TraitConstraint> constraints;
  if (match(TokenKind::KwWhere)) {
    do {
      const Token parameter = consume(TokenKind::Identifier,
                                      "expected constrained type parameter");
      consume(TokenKind::Colon, "expected ':' in trait constraint");
      const Token trait = consume(TokenKind::Identifier, "expected trait name");
      std::string traitName = trait.text;
      while (match(TokenKind::Dot))
        traitName += "." + consume(TokenKind::Identifier,
                                    "expected trait name after '.'").text;
      constraints.push_back({parameter.text, std::move(traitName), parameter.location});
    } while (match(TokenKind::Comma));
  }
  consume(TokenKind::Colon, "expected ':' before function body");
  consume(TokenKind::Newline, "expected newline after function signature");
  auto body = parseBlock();
  Function result{name.text, start.location, isPublic, std::move(typeParameters),
                  std::move(parameters), returnType, std::move(body)};
  result.constraints = std::move(constraints);
  return result;
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
  if (at(TokenKind::Identifier) && current().text == "unsafe" &&
      index_ + 1 < tokens_.size() && tokens_[index_ + 1].kind == TokenKind::Colon)
    return parseUnsafe();
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

std::unique_ptr<Stmt> Parser::parseUnsafe() {
  const Token keyword = consume(TokenKind::Identifier, "expected 'unsafe'");
  consume(TokenKind::Colon, "expected ':' after 'unsafe'");
  consume(TokenKind::Newline, "expected newline after 'unsafe'");
  return std::make_unique<UnsafeStmt>(keyword.location, parseBlock());
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
  const bool rangeLoop = match(TokenKind::DotDot);
  std::unique_ptr<Expr> end;
  if (rangeLoop) end = parseExpression();
  consume(TokenKind::Colon, "expected ':' after for source");
  consume(TokenKind::Newline, "expected newline after for source");
  return std::make_unique<ForStmt>(keyword.location, name.text, std::move(start),
                                   std::move(end), parseBlock(), rangeLoop);
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
  if (match(TokenKind::KwFn)) {
    const Token start = previous();
    consume(TokenKind::LParen, "expected '(' after 'fn' in lambda");
    std::vector<Parameter> parameters;
    if (!at(TokenKind::RParen)) {
      do {
        const Token parameter = consume(TokenKind::Identifier,
                                        "expected lambda parameter name");
        consume(TokenKind::Colon, "expected ':' after lambda parameter name");
        parameters.push_back({parameter.text, parseTypeName(), parameter.location});
      } while (match(TokenKind::Comma));
    }
    consume(TokenKind::RParen, "expected ')' after lambda parameters");
    consume(TokenKind::Arrow, "expected '->' and lambda result type");
    std::string result = parseTypeName();
    consume(TokenKind::FatArrow, "expected '=>' before lambda expression");
    return std::make_unique<LambdaExpr>(start.location, std::move(parameters),
                                        std::move(result), parseExpression());
  }
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
