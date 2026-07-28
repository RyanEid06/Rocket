#pragma once

#include <string>

namespace rocket {

enum class Type {
  Invalid,
  Int,
  Float,
  Bool,
  Char,
  String,
  Unit,
  ArrayInt,
  ArrayFloat,
  ArrayBool,
  ArrayChar,
  ArrayString,
  SliceInt,
  SliceFloat,
  SliceBool,
  SliceChar,
  SliceString,
};

Type typeFromName(const std::string& name);
const char* typeName(Type type);
bool isManagedType(Type type);
bool isArrayType(Type type);
bool isSliceType(Type type);
bool isCollectionType(Type type);
Type collectionElementType(Type type);
Type arrayType(Type element);
Type sliceType(Type element);

} // namespace rocket
