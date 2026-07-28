#include "type.h"

#include <cctype>

namespace rocket {

const Type Type::Invalid{TypeKind::Invalid};
const Type Type::Int{TypeKind::Int};
const Type Type::Float{TypeKind::Float};
const Type Type::Bool{TypeKind::Bool};
const Type Type::Char{TypeKind::Char};
const Type Type::String{TypeKind::String};
const Type Type::Unit{TypeKind::Unit};
const Type Type::ArrayInt{TypeKind::Array, "Array", {Type::Int}};
const Type Type::ArrayFloat{TypeKind::Array, "Array", {Type::Float}};
const Type Type::ArrayBool{TypeKind::Array, "Array", {Type::Bool}};
const Type Type::ArrayChar{TypeKind::Array, "Array", {Type::Char}};
const Type Type::ArrayString{TypeKind::Array, "Array", {Type::String}};
const Type Type::SliceInt{TypeKind::Slice, "Slice", {Type::Int}};
const Type Type::SliceFloat{TypeKind::Slice, "Slice", {Type::Float}};
const Type Type::SliceBool{TypeKind::Slice, "Slice", {Type::Bool}};
const Type Type::SliceChar{TypeKind::Slice, "Slice", {Type::Char}};
const Type Type::SliceString{TypeKind::Slice, "Slice", {Type::String}};

namespace {

class TypeParser {
public:
  explicit TypeParser(const std::string& text) : text_(text) {}

  Type parse() {
    Type result = parseType();
    skipSpaces();
    return index_ == text_.size() ? result : Type::Invalid;
  }

private:
  void skipSpaces() {
    while (index_ < text_.size() &&
           std::isspace(static_cast<unsigned char>(text_[index_])))
      ++index_;
  }

  std::string identifier() {
    skipSpaces();
    const std::size_t start = index_;
    while (index_ < text_.size() &&
           (std::isalnum(static_cast<unsigned char>(text_[index_])) ||
            text_[index_] == '_' || text_[index_] == '.'))
      ++index_;
    return text_.substr(start, index_ - start);
  }

  Type parseType() {
    const std::string name = identifier();
    if (name.empty()) return Type::Invalid;
    std::vector<Type> arguments;
    skipSpaces();
    if (index_ < text_.size() && text_[index_] == '[') {
      ++index_;
      do {
        Type argument = parseType();
        if (argument == Type::Invalid) return Type::Invalid;
        arguments.push_back(std::move(argument));
        skipSpaces();
        if (index_ < text_.size() && text_[index_] == ',') {
          ++index_;
          continue;
        }
        break;
      } while (true);
      skipSpaces();
      if (index_ >= text_.size() || text_[index_] != ']') return Type::Invalid;
      ++index_;
    }

    if (name == "Int" && arguments.empty()) return Type::Int;
    if (name == "Float" && arguments.empty()) return Type::Float;
    if (name == "Bool" && arguments.empty()) return Type::Bool;
    if (name == "Char" && arguments.empty()) return Type::Char;
    if (name == "String" && arguments.empty()) return Type::String;
    if (name == "Unit" && arguments.empty()) return Type::Unit;
    if (name == "Array" && arguments.size() == 1)
      return Type{TypeKind::Array, "Array", std::move(arguments)};
    if (name == "Slice" && arguments.size() == 1)
      return Type{TypeKind::Slice, "Slice", std::move(arguments)};
    return Type{TypeKind::Struct, name, std::move(arguments)};
  }

  const std::string& text_;
  std::size_t index_ = 0;
};

} // namespace

Type typeFromName(const std::string& name) { return TypeParser(name).parse(); }

std::string typeName(const Type& type) {
  switch (type.kind) {
  case TypeKind::Int: return "Int";
  case TypeKind::Float: return "Float";
  case TypeKind::Bool: return "Bool";
  case TypeKind::Char: return "Char";
  case TypeKind::String: return "String";
  case TypeKind::Unit: return "Unit";
  case TypeKind::Invalid: return "<invalid>";
  case TypeKind::Array: return "Array[" + typeName(type.arguments.at(0)) + "]";
  case TypeKind::Slice: return "Slice[" + typeName(type.arguments.at(0)) + "]";
  case TypeKind::Struct:
  case TypeKind::Enum:
  case TypeKind::TypeParameter: {
    std::string result = type.declaration;
    if (!type.arguments.empty()) {
      result += '[';
      for (std::size_t index = 0; index < type.arguments.size(); ++index) {
        if (index) result += ", ";
        result += typeName(type.arguments[index]);
      }
      result += ']';
    }
    return result;
  }
  }
  return "<invalid>";
}

bool isArrayType(const Type& type) { return type.kind == TypeKind::Array; }
bool isSliceType(const Type& type) { return type.kind == TypeKind::Slice; }
bool isCollectionType(const Type& type) { return isArrayType(type) || isSliceType(type); }
bool isAggregateType(const Type& type) {
  return type.kind == TypeKind::Struct || type.kind == TypeKind::Enum;
}
bool isManagedType(const Type& type) {
  return type == Type::String || isCollectionType(type) || isAggregateType(type);
}

Type collectionElementType(const Type& type) {
  return isCollectionType(type) && type.arguments.size() == 1
             ? type.arguments.front()
             : Type::Invalid;
}

Type arrayType(const Type& element) {
  return element == Type::Invalid || element == Type::Unit
             ? Type::Invalid
             : Type{TypeKind::Array, "Array", {element}};
}

Type sliceType(const Type& element) {
  return element == Type::Invalid || element == Type::Unit
             ? Type::Invalid
             : Type{TypeKind::Slice, "Slice", {element}};
}

} // namespace rocket
