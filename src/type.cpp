#include "type.h"

namespace rocket {

Type typeFromName(const std::string& name) {
  if (name == "Int") return Type::Int;
  if (name == "Float") return Type::Float;
  if (name == "Bool") return Type::Bool;
  if (name == "Char") return Type::Char;
  if (name == "String") return Type::String;
  if (name == "Unit") return Type::Unit;
  return Type::Invalid;
}

const char* typeName(Type type) {
  switch (type) {
  case Type::Int: return "Int";
  case Type::Float: return "Float";
  case Type::Bool: return "Bool";
  case Type::Char: return "Char";
  case Type::String: return "String";
  case Type::Unit: return "Unit";
  case Type::Invalid: return "<invalid>";
  }
  return "<invalid>";
}

} // namespace rocket
