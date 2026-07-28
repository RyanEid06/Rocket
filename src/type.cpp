#include "type.h"

namespace rocket {

Type typeFromName(const std::string& name) {
  if (name == "Int") return Type::Int;
  if (name == "Float") return Type::Float;
  if (name == "Bool") return Type::Bool;
  if (name == "Char") return Type::Char;
  if (name == "String") return Type::String;
  if (name == "Unit") return Type::Unit;
  if (name == "Array[Int]") return Type::ArrayInt;
  if (name == "Array[Float]") return Type::ArrayFloat;
  if (name == "Array[Bool]") return Type::ArrayBool;
  if (name == "Array[Char]") return Type::ArrayChar;
  if (name == "Array[String]") return Type::ArrayString;
  if (name == "Slice[Int]") return Type::SliceInt;
  if (name == "Slice[Float]") return Type::SliceFloat;
  if (name == "Slice[Bool]") return Type::SliceBool;
  if (name == "Slice[Char]") return Type::SliceChar;
  if (name == "Slice[String]") return Type::SliceString;
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
  case Type::ArrayInt: return "Array[Int]";
  case Type::ArrayFloat: return "Array[Float]";
  case Type::ArrayBool: return "Array[Bool]";
  case Type::ArrayChar: return "Array[Char]";
  case Type::ArrayString: return "Array[String]";
  case Type::SliceInt: return "Slice[Int]";
  case Type::SliceFloat: return "Slice[Float]";
  case Type::SliceBool: return "Slice[Bool]";
  case Type::SliceChar: return "Slice[Char]";
  case Type::SliceString: return "Slice[String]";
  case Type::Invalid: return "<invalid>";
  }
  return "<invalid>";
}

bool isArrayType(Type type) {
  return type >= Type::ArrayInt && type <= Type::ArrayString;
}

bool isSliceType(Type type) {
  return type >= Type::SliceInt && type <= Type::SliceString;
}

bool isCollectionType(Type type) { return isArrayType(type) || isSliceType(type); }

bool isManagedType(Type type) { return type == Type::String || isCollectionType(type); }

Type collectionElementType(Type type) {
  switch (type) {
  case Type::ArrayInt:
  case Type::SliceInt: return Type::Int;
  case Type::ArrayFloat:
  case Type::SliceFloat: return Type::Float;
  case Type::ArrayBool:
  case Type::SliceBool: return Type::Bool;
  case Type::ArrayChar:
  case Type::SliceChar: return Type::Char;
  case Type::ArrayString:
  case Type::SliceString: return Type::String;
  default: return Type::Invalid;
  }
}

Type arrayType(Type element) {
  switch (element) {
  case Type::Int: return Type::ArrayInt;
  case Type::Float: return Type::ArrayFloat;
  case Type::Bool: return Type::ArrayBool;
  case Type::Char: return Type::ArrayChar;
  case Type::String: return Type::ArrayString;
  default: return Type::Invalid;
  }
}

Type sliceType(Type element) {
  switch (element) {
  case Type::Int: return Type::SliceInt;
  case Type::Float: return Type::SliceFloat;
  case Type::Bool: return Type::SliceBool;
  case Type::Char: return Type::SliceChar;
  case Type::String: return Type::SliceString;
  default: return Type::Invalid;
  }
}

} // namespace rocket
