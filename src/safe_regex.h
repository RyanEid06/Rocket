#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rocket::safe_regex {

struct Match {
  std::size_t start = 0;
  std::size_t end = 0;
};

struct Result {
  bool valid = false;
  std::string error;
  std::vector<Match> matches;
};

namespace detail {

enum class NodeKind { Empty, Byte, Any, Class, Begin, End, Concat, Alternate, Star, Plus, Optional };

struct Node {
  explicit Node(NodeKind kind) : kind(kind) {}
  NodeKind kind;
  std::uint8_t byte = 0;
  std::array<bool, 256> bytes{};
  std::unique_ptr<Node> left;
  std::unique_ptr<Node> right;
};

class Parser {
public:
  explicit Parser(std::string_view pattern) : pattern_(pattern) {}

  std::unique_ptr<Node> parse() {
    if (pattern_.size() > 4096) {
      error_ = "regular expression exceeds the 4096-byte pattern limit";
      return nullptr;
    }
    auto result = alternate();
    if (!result) return nullptr;
    if (index_ != pattern_.size()) {
      error_ = pattern_[index_] == ')' ? "unmatched ')'" : "unexpected pattern byte";
      return nullptr;
    }
    return result;
  }

  const std::string& error() const { return error_; }

private:
  static std::unique_ptr<Node> combineBalanced(
      std::vector<std::unique_ptr<Node>>& parts, std::size_t begin,
      std::size_t end, NodeKind kind) {
    if (end - begin == 1) return std::move(parts[begin]);
    const std::size_t middle = begin + (end - begin) / 2;
    auto parent = std::make_unique<Node>(kind);
    parent->left = combineBalanced(parts, begin, middle, kind);
    parent->right = combineBalanced(parts, middle, end, kind);
    return parent;
  }

  std::unique_ptr<Node> alternate() {
    std::vector<std::unique_ptr<Node>> parts;
    auto first = concatenate();
    if (!first) return nullptr;
    parts.push_back(std::move(first));
    while (take('|')) {
      auto right = concatenate();
      if (!right) return nullptr;
      parts.push_back(std::move(right));
    }
    return combineBalanced(parts, 0, parts.size(), NodeKind::Alternate);
  }

  std::unique_ptr<Node> concatenate() {
    std::vector<std::unique_ptr<Node>> parts;
    while (index_ < pattern_.size() && pattern_[index_] != ')' &&
           pattern_[index_] != '|') {
      auto part = repetition();
      if (!part) return nullptr;
      parts.push_back(std::move(part));
    }
    if (parts.empty()) return std::make_unique<Node>(NodeKind::Empty);
    return combineBalanced(parts, 0, parts.size(), NodeKind::Concat);
  }

  std::unique_ptr<Node> repetition() {
    auto result = atom();
    if (!result) return nullptr;
    if (index_ >= pattern_.size()) return result;
    NodeKind kind;
    if (pattern_[index_] == '*') kind = NodeKind::Star;
    else if (pattern_[index_] == '+') kind = NodeKind::Plus;
    else if (pattern_[index_] == '?') kind = NodeKind::Optional;
    else return result;
    ++index_;
    auto parent = std::make_unique<Node>(kind);
    parent->left = std::move(result);
    if (index_ < pattern_.size() &&
        (pattern_[index_] == '*' || pattern_[index_] == '+' || pattern_[index_] == '?')) {
      error_ = "stacked repetition operators are not allowed";
      return nullptr;
    }
    return parent;
  }

  static void addEscape(std::array<bool, 256>& bytes, char escaped) {
    if (escaped == 'd') {
      for (unsigned byte = '0'; byte <= '9'; ++byte) bytes[byte] = true;
    } else if (escaped == 'w') {
      for (unsigned byte = '0'; byte <= '9'; ++byte) bytes[byte] = true;
      for (unsigned byte = 'A'; byte <= 'Z'; ++byte) bytes[byte] = true;
      for (unsigned byte = 'a'; byte <= 'z'; ++byte) bytes[byte] = true;
      bytes[static_cast<unsigned>('_')] = true;
    } else if (escaped == 's') {
      for (unsigned byte : {' ', '\t', '\r', '\n', '\f', '\v'}) bytes[byte] = true;
    }
  }

  bool classByte(std::array<bool, 256>& destination, unsigned& byte,
                 bool& isSet) {
    if (index_ >= pattern_.size()) {
      error_ = "unterminated character class";
      return false;
    }
    unsigned value = static_cast<unsigned char>(pattern_[index_++]);
    if (value != '\\') {
      byte = value;
      isSet = false;
      return true;
    }
    if (index_ >= pattern_.size()) {
      error_ = "unterminated escape";
      return false;
    }
    const char escaped = pattern_[index_++];
    if (escaped == 'd' || escaped == 'w' || escaped == 's') {
      addEscape(destination, escaped);
      isSet = true;
      return true;
    }
    byte = escaped == 'n' ? '\n' : escaped == 'r' ? '\r' :
           escaped == 't' ? '\t' : static_cast<unsigned char>(escaped);
    isSet = false;
    return true;
  }

  std::unique_ptr<Node> characterClass() {
    ++index_;
    auto result = std::make_unique<Node>(NodeKind::Class);
    const bool negate = take('^');
    bool any = false;
    while (index_ < pattern_.size() && pattern_[index_] != ']') {
      unsigned first = 0;
      bool firstSet = false;
      if (!classByte(result->bytes, first, firstSet)) return nullptr;
      any = true;
      if (!firstSet && index_ + 1 < pattern_.size() && pattern_[index_] == '-' &&
          pattern_[index_ + 1] != ']') {
        ++index_;
        unsigned last = 0;
        bool lastSet = false;
        if (!classByte(result->bytes, last, lastSet)) return nullptr;
        if (lastSet || first > last) {
          error_ = "invalid character-class range";
          return nullptr;
        }
        for (unsigned byte = first; byte <= last; ++byte) result->bytes[byte] = true;
      } else if (!firstSet) {
        result->bytes[first] = true;
      }
    }
    if (!take(']')) {
      error_ = "unterminated character class";
      return nullptr;
    }
    if (!any) {
      error_ = "empty character class";
      return nullptr;
    }
    if (negate)
      for (std::size_t byte = 0; byte < result->bytes.size(); ++byte)
        result->bytes[byte] = !result->bytes[byte];
    return result;
  }

  std::unique_ptr<Node> atom() {
    if (index_ >= pattern_.size()) {
      error_ = "missing expression";
      return nullptr;
    }
    const char character = pattern_[index_++];
    if (character == '(') {
      if (groupDepth_ == 256) {
        error_ = "regular-expression group nesting exceeds 256";
        return nullptr;
      }
      ++groupDepth_;
      auto result = alternate();
      --groupDepth_;
      if (!result) return nullptr;
      if (!take(')')) {
        error_ = "unmatched '('";
        return nullptr;
      }
      return result;
    }
    if (character == '[') {
      --index_;
      return characterClass();
    }
    if (character == '.' || character == '^' || character == '$') {
      return std::make_unique<Node>(character == '.' ? NodeKind::Any :
                                    character == '^' ? NodeKind::Begin : NodeKind::End);
    }
    if (character == '*' || character == '+' || character == '?') {
      error_ = "repetition operator has no preceding expression";
      return nullptr;
    }
    auto result = std::make_unique<Node>(NodeKind::Byte);
    if (character != '\\') {
      result->byte = static_cast<std::uint8_t>(character);
      return result;
    }
    if (index_ >= pattern_.size()) {
      error_ = "unterminated escape";
      return nullptr;
    }
    const char escaped = pattern_[index_++];
    if (escaped == 'd' || escaped == 'w' || escaped == 's') {
      result->kind = NodeKind::Class;
      addEscape(result->bytes, escaped);
    } else {
      result->byte = escaped == 'n' ? '\n' : escaped == 'r' ? '\r' :
                     escaped == 't' ? '\t' : static_cast<std::uint8_t>(escaped);
    }
    return result;
  }

  bool take(char expected) {
    if (index_ >= pattern_.size() || pattern_[index_] != expected) return false;
    ++index_;
    return true;
  }

  std::string_view pattern_;
  std::size_t index_ = 0;
  std::size_t groupDepth_ = 0;
  std::string error_;
};

enum class StateKind { Byte, Any, Class, Begin, End, Split, Epsilon, Accept };

struct State {
  StateKind kind;
  int out = -1;
  int alternate = -1;
  std::uint8_t byte = 0;
  std::array<bool, 256> bytes{};
};

struct Patch { int state; bool alternate; };
struct Fragment { int start; std::vector<Patch> exits; };

class Program {
public:
  bool compile(std::unique_ptr<Node> root, std::string& error) {
    Fragment fragment = emit(*root);
    if (states_.size() >= 16384) {
      error = "regular expression exceeds the compiled-state limit";
      return false;
    }
    const int accept = add({StateKind::Accept});
    patch(fragment.exits, accept);
    start_ = fragment.start;
    accept_ = static_cast<std::size_t>(accept);
    return true;
  }

  bool find(std::string_view input, std::size_t from, Match& result) const {
    if (from > input.size()) return false;
    constexpr std::size_t inactive = static_cast<std::size_t>(-1);
    std::vector<std::size_t> current(states_.size(), inactive);
    bool accepted = false;
    for (std::size_t position = from; position <= input.size(); ++position) {
      closure(current, start_, position, position, input.size());
      const std::size_t acceptedStart = current[accept_];
      if (acceptedStart != inactive &&
          (!accepted || acceptedStart < result.start)) {
        result = {acceptedStart, position};
        accepted = true;
      } else if (acceptedStart != inactive && acceptedStart == result.start) {
        result.end = position;
      }
      if (position == input.size()) break;

      std::vector<std::size_t> next(states_.size(), inactive);
      std::vector<Seed> seeds;
      const auto byte = static_cast<std::uint8_t>(input[position]);
      for (std::size_t index = 0; index < states_.size(); ++index) {
        if (current[index] == inactive) continue;
        const State& state = states_[index];
        if ((state.kind == StateKind::Byte && state.byte == byte) ||
            state.kind == StateKind::Any ||
            (state.kind == StateKind::Class && state.bytes[byte]))
          seeds.push_back({state.out, current[index]});
      }
      closureMany(next, seeds, position + 1, input.size());
      current = std::move(next);
    }
    return accepted;
  }

private:
  int add(State state) {
    states_.push_back(std::move(state));
    return static_cast<int>(states_.size() - 1);
  }

  void patch(const std::vector<Patch>& exits, int target) {
    for (const Patch exit : exits) {
      State& state = states_[static_cast<std::size_t>(exit.state)];
      (exit.alternate ? state.alternate : state.out) = target;
    }
  }

  static std::vector<Patch> joined(std::vector<Patch> left,
                                   const std::vector<Patch>& right) {
    left.insert(left.end(), right.begin(), right.end());
    return left;
  }

  Fragment emit(const Node& node) {
    if (node.kind == NodeKind::Concat) {
      Fragment left = emit(*node.left);
      Fragment right = emit(*node.right);
      patch(left.exits, right.start);
      return {left.start, std::move(right.exits)};
    }
    if (node.kind == NodeKind::Alternate) {
      Fragment left = emit(*node.left);
      Fragment right = emit(*node.right);
      const int split = add({StateKind::Split, left.start, right.start});
      return {split, joined(std::move(left.exits), right.exits)};
    }
    if (node.kind == NodeKind::Star || node.kind == NodeKind::Plus ||
        node.kind == NodeKind::Optional) {
      Fragment child = emit(*node.left);
      const int split = add({StateKind::Split, child.start, -1});
      if (node.kind != NodeKind::Optional) patch(child.exits, split);
      if (node.kind == NodeKind::Star) return {split, {{split, true}}};
      if (node.kind == NodeKind::Plus) return {child.start, {{split, true}}};
      return {split, joined(std::move(child.exits), {{split, true}})};
    }
    State state{node.kind == NodeKind::Byte ? StateKind::Byte :
                node.kind == NodeKind::Any ? StateKind::Any :
                node.kind == NodeKind::Class ? StateKind::Class :
                node.kind == NodeKind::Begin ? StateKind::Begin :
                node.kind == NodeKind::End ? StateKind::End : StateKind::Epsilon};
    state.byte = node.byte;
    state.bytes = node.bytes;
    const int index = add(std::move(state));
    return {index, {{index, false}}};
  }

  struct Seed { int state; std::size_t start; };

  void closure(std::vector<std::size_t>& active, int initial,
               std::size_t start, std::size_t position,
               std::size_t length) const {
    std::vector<Seed> seeds{{initial, start}};
    closureMany(active, seeds, position, length);
  }

  void closureMany(std::vector<std::size_t>& active,
                   std::vector<Seed>& pending, std::size_t position,
                   std::size_t length) const {
    constexpr std::size_t inactive = static_cast<std::size_t>(-1);
    std::sort(pending.begin(), pending.end(), [](const Seed& left, const Seed& right) {
      return left.start > right.start;
    });
    while (!pending.empty()) {
      const Seed seed = pending.back();
      pending.pop_back();
      const int index = seed.state;
      if (index < 0) continue;
      const std::size_t stateIndex = static_cast<std::size_t>(index);
      if (active[stateIndex] != inactive && active[stateIndex] <= seed.start) continue;
      active[stateIndex] = seed.start;
      const State& state = states_[stateIndex];
      if (state.kind == StateKind::Split) {
        pending.push_back({state.alternate, seed.start});
        pending.push_back({state.out, seed.start});
      } else if (state.kind == StateKind::Epsilon ||
                 (state.kind == StateKind::Begin && position == 0) ||
                 (state.kind == StateKind::End && position == length)) {
        pending.push_back({state.out, seed.start});
      }
    }
  }

  std::vector<State> states_;
  int start_ = -1;
  std::size_t accept_ = 0;
};

inline bool prepare(std::string_view pattern, Program& program, std::string& error) {
  Parser parser(pattern);
  auto root = parser.parse();
  if (!root) {
    error = "invalid regular expression: " + parser.error();
    return false;
  }
  return program.compile(std::move(root), error);
}

} // namespace detail

inline Result findAll(std::string_view pattern, std::string_view input) {
  Result result;
  if (input.size() > 16 * 1024 * 1024) {
    result.error = "regular-expression input exceeds the 16 MiB limit";
    return result;
  }
  detail::Program program;
  if (!detail::prepare(pattern, program, result.error)) return result;
  result.valid = true;
  std::size_t cursor = 0;
  while (cursor <= input.size()) {
    Match match;
    if (!program.find(input, cursor, match)) break;
    result.matches.push_back(match);
    if (result.matches.size() > 1000000) {
      result.valid = false;
      result.error = "regular expression produced more than 1000000 matches";
      result.matches.clear();
      return result;
    }
    cursor = match.end > match.start ? match.end : match.end + 1;
  }
  return result;
}

inline Result search(std::string_view pattern, std::string_view input) {
  Result result;
  if (input.size() > 16 * 1024 * 1024) {
    result.error = "regular-expression input exceeds the 16 MiB limit";
    return result;
  }
  detail::Program program;
  if (!detail::prepare(pattern, program, result.error)) return result;
  result.valid = true;
  Match match;
  if (program.find(input, 0, match)) result.matches.push_back(match);
  return result;
}

inline bool replaceAll(std::string_view pattern, std::string_view input,
                       std::string_view replacement, std::string& output,
                       std::string& error) {
  if (replacement.size() > 16 * 1024 * 1024) {
    error = "regular-expression replacement exceeds the 16 MiB limit";
    return false;
  }
  Result found = findAll(pattern, input);
  if (!found.valid) {
    error = std::move(found.error);
    return false;
  }
  std::size_t cursor = 0;
  for (const Match match : found.matches) {
    if (match.start > cursor) output.append(input.substr(cursor, match.start - cursor));
    output.append(replacement);
    cursor = match.end;
    if (output.size() > 64 * 1024 * 1024) {
      error = "regular-expression replacement output exceeds the 64 MiB limit";
      output.clear();
      return false;
    }
  }
  output.append(input.substr(cursor));
  if (output.size() > 64 * 1024 * 1024) {
    error = "regular-expression replacement output exceeds the 64 MiB limit";
    output.clear();
    return false;
  }
  return true;
}

} // namespace rocket::safe_regex
