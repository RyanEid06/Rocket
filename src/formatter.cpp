#include "formatter.h"

#include "lexer.h"

#include <algorithm>
#include <map>
#include <sstream>
#include <string_view>
#include <vector>

namespace rocket {
namespace {

bool word(TokenKind kind) {
  return kind == TokenKind::Identifier || kind == TokenKind::Integer ||
         kind == TokenKind::Float || kind == TokenKind::Character ||
         kind == TokenKind::String ||
         (kind >= TokenKind::KwFn && kind <= TokenKind::KwImport);
}

bool binary(TokenKind kind) {
  return kind == TokenKind::Plus || kind == TokenKind::Minus ||
         kind == TokenKind::Star || kind == TokenKind::Slash ||
         kind == TokenKind::Equal || kind == TokenKind::EqualEqual ||
         kind == TokenKind::BangEqual || kind == TokenKind::Less ||
         kind == TokenKind::LessEqual || kind == TokenKind::Greater ||
         kind == TokenKind::GreaterEqual || kind == TokenKind::Arrow ||
         kind == TokenKind::FatArrow ||
         kind == TokenKind::KwAnd || kind == TokenKind::KwOr;
}

bool prefixContext(TokenKind kind) {
  return kind == TokenKind::LParen || kind == TokenKind::LBracket ||
         kind == TokenKind::Comma || kind == TokenKind::Equal ||
         kind == TokenKind::Colon || kind == TokenKind::KwReturn ||
         kind == TokenKind::KwIn || binary(kind);
}

std::string escapedString(std::string_view value, char quote) {
  std::string result(1, quote);
  for (const char character : value) {
    if (character == '\\') result += "\\\\";
    else if (character == quote) { result.push_back('\\'); result.push_back(quote); }
    else if (character == '\n') result += "\\n";
    else if (character == '\t') result += "\\t";
    else if (character == '\r') result += "\\r";
    else result.push_back(character);
  }
  result.push_back(quote);
  return result;
}

std::string spelling(const Token& token) {
  if (token.kind == TokenKind::String) return escapedString(token.text, '"');
  if (token.kind == TokenKind::Character) return escapedString(token.text, '\'');
  return token.text.empty() ? tokenName(token.kind) : token.text;
}

std::size_t commentOffset(const std::string& line) {
  bool string = false;
  bool character = false;
  bool escaped = false;
  for (std::size_t index = 0; index < line.size(); ++index) {
    const char value = line[index];
    if (escaped) { escaped = false; continue; }
    if ((string || character) && value == '\\') { escaped = true; continue; }
    if (!character && value == '"') { string = !string; continue; }
    if (!string && value == '\'') { character = !character; continue; }
    if (!string && !character && value == '#') return index;
  }
  return std::string::npos;
}

std::string trimRight(std::string value) {
  while (!value.empty() && (value.back() == ' ' || value.back() == '\t' ||
                            value.back() == '\r')) value.pop_back();
  return value;
}

bool unaryMinus(const std::vector<Token>& tokens, std::size_t index) {
  if (tokens[index].kind != TokenKind::Minus) return false;
  return index == 0 || prefixContext(tokens[index - 1].kind);
}

bool needsSpace(const std::vector<Token>& tokens, std::size_t index) {
  if (index == 0) return false;
  const TokenKind current = tokens[index].kind;
  const TokenKind previous = tokens[index - 1].kind;
  if (current == TokenKind::RParen || current == TokenKind::RBracket ||
      current == TokenKind::Comma || current == TokenKind::Colon ||
      current == TokenKind::Dot || current == TokenKind::DotDot ||
      current == TokenKind::Question)
    return false;
  if (previous == TokenKind::LParen || previous == TokenKind::LBracket ||
      previous == TokenKind::Dot || previous == TokenKind::DotDot)
    return false;
  if (current == TokenKind::Identifier && previous == TokenKind::RBracket &&
      !tokens.empty() && tokens[0].kind == TokenKind::KwImpl)
    return true;
  if (current == TokenKind::LParen) return false;
  if (current == TokenKind::LBracket &&
      (word(previous) || previous == TokenKind::RParen ||
       previous == TokenKind::RBracket || previous == TokenKind::Question))
    return false;
  if (previous == TokenKind::Comma || previous == TokenKind::Colon) return true;
  if (unaryMinus(tokens, index)) return true;
  if (unaryMinus(tokens, index - 1)) return false;
  if (binary(current) || binary(previous)) return true;
  if (current == TokenKind::KwNot || previous == TokenKind::KwNot) return true;
  return word(previous) && word(current);
}

} // namespace

std::optional<std::string> formatSource(const std::string& file,
                                        const std::string& source,
                                        Diagnostics& diagnostics) {
  Lexer lexer(file, source, diagnostics);
  const std::vector<Token> all = lexer.lex();
  if (diagnostics.hasErrors()) return std::nullopt;

  std::map<int, std::vector<Token>> byLine;
  for (const auto& token : all)
    if (token.kind != TokenKind::Indent && token.kind != TokenKind::Dedent &&
        token.kind != TokenKind::Newline && token.kind != TokenKind::End)
      byLine[token.location.line].push_back(token);

  std::vector<std::string> lines;
  std::istringstream input(source);
  std::string line;
  while (std::getline(input, line)) lines.push_back(trimRight(std::move(line)));
  if (lines.empty() && !source.empty()) lines.push_back(source);

  std::ostringstream output;
  for (std::size_t lineIndex = 0; lineIndex < lines.size(); ++lineIndex) {
    const std::string& original = lines[lineIndex];
    std::size_t leading = 0;
    while (leading < original.size() && original[leading] == ' ') ++leading;
    const std::size_t comment = commentOffset(original);
    const auto found = byLine.find(static_cast<int>(lineIndex + 1));
    const bool hasCode = found != byLine.end() && !found->second.empty();

    if (hasCode) {
      output << std::string((leading / 4) * 4, ' ');
      const auto& tokens = found->second;
      for (std::size_t index = 0; index < tokens.size(); ++index) {
        if (needsSpace(tokens, index)) output << ' ';
        output << spelling(tokens[index]);
      }
      if (comment != std::string::npos)
        output << "  " << trimRight(original.substr(comment));
    } else if (comment != std::string::npos) {
      output << std::string((leading / 4) * 4, ' ')
             << trimRight(original.substr(comment));
    }
    output << '\n';
  }
  if (lines.empty()) output << '\n';
  return output.str();
}

} // namespace rocket
