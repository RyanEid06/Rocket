#include "lexer.h"

#include <cctype>
#include <sstream>
#include <unordered_map>

namespace rocket {

const char* tokenName(TokenKind kind) {
  switch (kind) {
  case TokenKind::End: return "end of file";
  case TokenKind::Newline: return "newline";
  case TokenKind::Indent: return "indent";
  case TokenKind::Dedent: return "dedent";
  case TokenKind::Identifier: return "identifier";
  case TokenKind::Integer: return "integer";
  case TokenKind::Float: return "float";
  case TokenKind::Character: return "character";
  case TokenKind::String: return "string";
  case TokenKind::KwFn: return "fn";
  case TokenKind::KwLet: return "let";
  case TokenKind::KwVar: return "var";
  case TokenKind::KwIf: return "if";
  case TokenKind::KwElse: return "else";
  case TokenKind::KwWhile: return "while";
  case TokenKind::KwFor: return "for";
  case TokenKind::KwIn: return "in";
  case TokenKind::KwBreak: return "break";
  case TokenKind::KwContinue: return "continue";
  case TokenKind::KwReturn: return "return";
  case TokenKind::KwTrue: return "true";
  case TokenKind::KwFalse: return "false";
  case TokenKind::KwAnd: return "and";
  case TokenKind::KwOr: return "or";
  case TokenKind::KwNot: return "not";
  case TokenKind::KwStruct: return "struct";
  case TokenKind::KwEnum: return "enum";
  case TokenKind::KwImpl: return "impl";
  case TokenKind::KwMatch: return "match";
  case TokenKind::KwCase: return "case";
  case TokenKind::KwPub: return "pub";
  case TokenKind::KwImport: return "import";
  case TokenKind::LParen: return "(";
  case TokenKind::RParen: return ")";
  case TokenKind::LBracket: return "[";
  case TokenKind::RBracket: return "]";
  case TokenKind::Colon: return ":";
  case TokenKind::Comma: return ",";
  case TokenKind::Arrow: return "->";
  case TokenKind::Plus: return "+";
  case TokenKind::Minus: return "-";
  case TokenKind::Star: return "*";
  case TokenKind::Slash: return "/";
  case TokenKind::Dot: return ".";
  case TokenKind::DotDot: return "..";
  case TokenKind::Question: return "?";
  case TokenKind::Equal: return "=";
  case TokenKind::EqualEqual: return "==";
  case TokenKind::BangEqual: return "!=";
  case TokenKind::Less: return "<";
  case TokenKind::LessEqual: return "<=";
  case TokenKind::Greater: return ">";
  case TokenKind::GreaterEqual: return ">=";
  }
  return "token";
}

void Lexer::emit(TokenKind kind, std::string text, int line, int column) {
  tokens_.push_back({kind, std::move(text), {file_, line, column}});
}

void Lexer::scanLine(const std::string& text, int lineNumber, std::size_t start) {
  static const std::unordered_map<std::string, TokenKind> keywords = {
      {"fn", TokenKind::KwFn}, {"let", TokenKind::KwLet},
      {"var", TokenKind::KwVar}, {"if", TokenKind::KwIf},
      {"else", TokenKind::KwElse}, {"while", TokenKind::KwWhile},
      {"for", TokenKind::KwFor}, {"in", TokenKind::KwIn},
      {"break", TokenKind::KwBreak}, {"continue", TokenKind::KwContinue},
      {"return", TokenKind::KwReturn}, {"true", TokenKind::KwTrue},
      {"false", TokenKind::KwFalse}, {"and", TokenKind::KwAnd},
      {"or", TokenKind::KwOr}, {"not", TokenKind::KwNot},
      {"struct", TokenKind::KwStruct}, {"enum", TokenKind::KwEnum},
      {"impl", TokenKind::KwImpl},
      {"match", TokenKind::KwMatch}, {"case", TokenKind::KwCase},
      {"pub", TokenKind::KwPub}, {"import", TokenKind::KwImport}};

  std::size_t i = start;
  while (i < text.size()) {
    const int column = static_cast<int>(i + 1);
    const char c = text[i];
    if (c == ' ' || c == '\r') { ++i; continue; }
    if (c == '#') break;
    if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
      const std::size_t begin = i++;
      while (i < text.size() &&
             (std::isalnum(static_cast<unsigned char>(text[i])) || text[i] == '_')) ++i;
      std::string word = text.substr(begin, i - begin);
      auto found = keywords.find(word);
      emit(found == keywords.end() ? TokenKind::Identifier : found->second,
           std::move(word), lineNumber, column);
      continue;
    }
    if (std::isdigit(static_cast<unsigned char>(c))) {
      const std::size_t begin = i++;
      while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
      TokenKind kind = TokenKind::Integer;
      if (i + 1 < text.size() && text[i] == '.' && text[i + 1] != '.' &&
          std::isdigit(static_cast<unsigned char>(text[i + 1]))) {
        kind = TokenKind::Float;
        ++i;
        while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i]))) ++i;
      }
      emit(kind, text.substr(begin, i - begin), lineNumber, column);
      continue;
    }
    if (c == '\'') {
      ++i;
      std::string value;
      bool closed = false;
      if (i < text.size() && text[i] == '\\' && i + 1 < text.size()) {
        const char escaped = text[++i];
        switch (escaped) {
        case 'n': value.push_back('\n'); break;
        case 'r': value.push_back('\r'); break;
        case 't': value.push_back('\t'); break;
        case '\'': value.push_back('\''); break;
        case '\\': value.push_back('\\'); break;
        default: diagnostics_.error({file_, lineNumber, static_cast<int>(i + 1)},
                                    "unknown character escape", DiagnosticCode::Lexical); value.push_back(escaped); break;
        }
        ++i;
      } else if (i < text.size() && text[i] != '\'' && text[i] != '\r') {
        value.push_back(text[i++]);
      }
      if (i < text.size() && text[i] == '\'') { ++i; closed = true; }
      if (!closed || value.size() != 1) {
        diagnostics_.error({file_, lineNumber, column},
                           "character literals must contain exactly one character",
                           DiagnosticCode::Lexical);
        while (i < text.size() && text[i] != '\'' && text[i] != '\r') ++i;
        if (i < text.size() && text[i] == '\'') ++i;
      }
      emit(TokenKind::Character, std::move(value), lineNumber, column);
      continue;
    }
    if (c == '"') {
      ++i;
      std::string value;
      bool closed = false;
      while (i < text.size()) {
        if (text[i] == '"') { ++i; closed = true; break; }
        if (text[i] == '\\' && i + 1 < text.size()) {
          const char escaped = text[++i];
          switch (escaped) {
          case 'n': value.push_back('\n'); break;
          case 'r': value.push_back('\r'); break;
          case 't': value.push_back('\t'); break;
          case '"': value.push_back('"'); break;
          case '\\': value.push_back('\\'); break;
          default:
            diagnostics_.error({file_, lineNumber, static_cast<int>(i + 1)},
                               "unknown string escape", DiagnosticCode::Lexical);
            value.push_back(escaped);
          }
          ++i;
        } else {
          value.push_back(text[i++]);
        }
      }
      if (!closed) diagnostics_.error({file_, lineNumber, column},
                                      "unterminated string literal",
                                      DiagnosticCode::Lexical);
      emit(TokenKind::String, std::move(value), lineNumber, column);
      continue;
    }

    auto two = [&](char second, TokenKind paired, TokenKind single) {
      if (i + 1 < text.size() && text[i + 1] == second) {
        emit(paired, text.substr(i, 2), lineNumber, column); i += 2;
      } else { emit(single, text.substr(i, 1), lineNumber, column); ++i; }
    };
    switch (c) {
    case '(': emit(TokenKind::LParen, "(", lineNumber, column); ++i; break;
    case ')': emit(TokenKind::RParen, ")", lineNumber, column); ++i; break;
    case '[': emit(TokenKind::LBracket, "[", lineNumber, column); ++i; break;
    case ']': emit(TokenKind::RBracket, "]", lineNumber, column); ++i; break;
    case ':': emit(TokenKind::Colon, ":", lineNumber, column); ++i; break;
    case ',': emit(TokenKind::Comma, ",", lineNumber, column); ++i; break;
    case '+': emit(TokenKind::Plus, "+", lineNumber, column); ++i; break;
    case '*': emit(TokenKind::Star, "*", lineNumber, column); ++i; break;
    case '/': emit(TokenKind::Slash, "/", lineNumber, column); ++i; break;
    case '?': emit(TokenKind::Question, "?", lineNumber, column); ++i; break;
    case '.':
      if (i + 1 < text.size() && text[i + 1] == '.') {
        emit(TokenKind::DotDot, "..", lineNumber, column); i += 2;
      } else {
        emit(TokenKind::Dot, ".", lineNumber, column); ++i;
      }
      break;
    case '-': two('>', TokenKind::Arrow, TokenKind::Minus); break;
    case '=': two('=', TokenKind::EqualEqual, TokenKind::Equal); break;
    case '<': two('=', TokenKind::LessEqual, TokenKind::Less); break;
    case '>': two('=', TokenKind::GreaterEqual, TokenKind::Greater); break;
    case '!':
      if (i + 1 < text.size() && text[i + 1] == '=') {
        emit(TokenKind::BangEqual, "!=", lineNumber, column); i += 2;
      } else {
        diagnostics_.error({file_, lineNumber, column}, "expected '=' after '!'",
                           DiagnosticCode::Lexical); ++i;
      }
      break;
    case '\t':
      diagnostics_.error({file_, lineNumber, column}, "tabs are not allowed; use spaces",
                         DiagnosticCode::Indentation); ++i; break;
    default:
      diagnostics_.error({file_, lineNumber, column},
                         std::string("unexpected character '") + c + "'",
                         DiagnosticCode::Lexical); ++i;
    }
  }
}

std::vector<Token> Lexer::lex() {
  std::istringstream input(source_);
  std::string lineText;
  int lineNumber = 1;
  while (std::getline(input, lineText)) {
    if (!lineText.empty() && lineText.back() == '\r') lineText.pop_back();
    std::size_t first = 0;
    while (first < lineText.size() && lineText[first] == ' ') ++first;
    if (first < lineText.size() && lineText[first] == '\t') {
      diagnostics_.error({file_, lineNumber, static_cast<int>(first + 1)},
                         "tabs are not allowed for indentation",
                         DiagnosticCode::Indentation);
      while (first < lineText.size() && lineText[first] == '\t') ++first;
    }
    const bool blank = first >= lineText.size() || lineText[first] == '#';
    if (!blank) {
      const int indent = static_cast<int>(first);
      if (indent % 4 != 0) {
        diagnostics_.error({file_, lineNumber, 1},
                           "indentation must use multiples of four spaces",
                           DiagnosticCode::Indentation);
      }
      if (indent > indentStack_.back()) {
        indentStack_.push_back(indent);
        emit(TokenKind::Indent, "", lineNumber, 1);
      } else {
        while (indent < indentStack_.back()) {
          indentStack_.pop_back();
          emit(TokenKind::Dedent, "", lineNumber, 1);
        }
        if (indent != indentStack_.back()) {
          diagnostics_.error({file_, lineNumber, 1},
                             "indentation does not match an outer block",
                             DiagnosticCode::Indentation);
        }
      }
      scanLine(lineText, lineNumber, first);
      emit(TokenKind::Newline, "", lineNumber, static_cast<int>(lineText.size() + 1));
    }
    ++lineNumber;
  }
  while (indentStack_.size() > 1) {
    indentStack_.pop_back();
    emit(TokenKind::Dedent, "", lineNumber, 1);
  }
  emit(TokenKind::End, "", lineNumber, 1);
  return tokens_;
}

} // namespace rocket
