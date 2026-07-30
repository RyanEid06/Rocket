#pragma once

#include <string>
#include <vector>

namespace rocket {

// Types are structural values. Built-in scalars are leaf nodes, while
// collections and user declarations carry recursively nested arguments.
// Keeping Type as a value (rather than an arena index) makes HIR/MIR dumps and
// generic-specialization keys deterministic across compiler runs.
enum class TypeKind {
  Invalid,
  Int,
  Float,
  Bool,
  Char,
  String,
  Unit,
  Array,
  Slice,
  Pointer,
  Struct,
  Enum,
  NativeStruct,
  Opaque,
  Callback,
  TypeParameter,
};

struct Type {
  TypeKind kind = TypeKind::Invalid;
  std::string declaration;
  std::vector<Type> arguments;

  Type() = default;
  explicit Type(TypeKind kind) : kind(kind) {}
  Type(TypeKind kind, std::string declaration, std::vector<Type> arguments = {})
      : kind(kind), declaration(std::move(declaration)), arguments(std::move(arguments)) {}

  static const Type Invalid;
  static const Type Int;
  static const Type Float;
  static const Type Bool;
  static const Type Char;
  static const Type String;
  static const Type Unit;
  // Compatibility spellings for the concrete Phase 5 collection ABI. They
  // are structural Array/Slice nodes, not additional closed enum cases.
  static const Type ArrayInt;
  static const Type ArrayFloat;
  static const Type ArrayBool;
  static const Type ArrayChar;
  static const Type ArrayString;
  static const Type SliceInt;
  static const Type SliceFloat;
  static const Type SliceBool;
  static const Type SliceChar;
  static const Type SliceString;

  friend bool operator==(const Type&, const Type&) = default;
};

// Parses built-in and structural syntax. Unknown names are represented as
// nominal types and are validated against declarations by semantic analysis.
Type typeFromName(const std::string& name);
std::string typeName(const Type& type);
bool isManagedType(const Type& type);
bool isArrayType(const Type& type);
bool isSliceType(const Type& type);
bool isCollectionType(const Type& type);
bool isAggregateType(const Type& type);
bool isNativeType(const Type& type);
bool isNativeAbiValueType(const Type& type);
bool isPointerType(const Type& type);
Type collectionElementType(const Type& type);
Type arrayType(const Type& element);
Type sliceType(const Type& element);

} // namespace rocket
