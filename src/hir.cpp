#include "hir.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <sstream>
#include <unordered_set>
#include <utility>

namespace rocket {

namespace {

Type typeParameter(const std::string& name) {
  return Type{TypeKind::TypeParameter, name};
}

bool containsTypeParameter(const Type& type) {
  if (type.kind == TypeKind::TypeParameter) return true;
  for (const auto& argument : type.arguments)
    if (containsTypeParameter(argument)) return true;
  return false;
}

bool isHashableKey(const Type& type) {
  return type == Type::Int || type == Type::Bool || type == Type::Char ||
         type == Type::String;
}

bool isCollectionComparable(const Type& type) {
  return isHashableKey(type) || type == Type::Float;
}

std::string associatedLibraryFunction(const std::string& name) {
  if (name == "String.from_int") return "std.string.from_int";
  if (name == "String.builder") return "std.string.builder";
  if (name == "std.collections.Map.from_arrays")
    return "std.collections.map_from_arrays";
  if (name == "std.collections.Set.from_array")
    return "std.collections.set_from_array";
  return name;
}

std::string libraryMethodFunction(const Type& receiver, const std::string& method) {
  if (receiver == Type::String) {
    if (method == "byte_length" || method == "concat" || method == "contains" ||
        method == "starts_with" || method == "ends_with" || method == "trim" ||
        method == "split" || method == "byte_at" || method == "byte_value_at" ||
        method == "slice" || method == "parse_int")
      return "std.string." + method;
  }
  if (isSliceType(receiver) && method == "length")
    return "std.collections.slice_length";
  if (isArrayType(receiver)) {
    if (method == "length" || method == "contains" || method == "find" ||
        method == "filter_equal" || method == "reverse" || method == "join")
      return "std.collections." + method;
    if (method == "capacity" || method == "reserve" || method == "append" ||
        method == "pop" || method == "insert" || method == "remove" ||
        method == "clear")
      return "std.collections." + method;
    if (method == "sort") {
      const Type element = collectionElementType(receiver);
      if (element == Type::Int) return "std.collections.sort_int";
      if (element == Type::Float) return "std.collections.sort_float";
      if (element == Type::Char) return "std.collections.sort_char";
      if (element == Type::String) return "std.collections.sort_string";
    }
  }
  if (receiver.declaration == "std.collections.Map") {
    if (method == "length" || method == "find" || method == "get" ||
        method == "keys" || method == "values")
      return "std.collections.map_" + method;
  }
  if (receiver.declaration == "std.collections.Set") {
    if (method == "contains" || method == "values")
      return "std.collections.set_" + method;
  }
  return {};
}

} // namespace

SymbolId HirLowerer::addSymbol(SymbolKind kind, const std::string& name, Type type,
                               bool mutableBinding, const Location& location,
                               std::vector<Type> parameterTypes, Intrinsic intrinsic) {
  const SymbolId id = static_cast<SymbolId>(hir_.symbols.size());
  hir_.symbols.push_back({id, kind, name, std::move(type), mutableBinding, location,
                          std::move(parameterTypes), intrinsic});
  return id;
}

void HirLowerer::registerBuiltinTypes() {
  HirTypeDeclaration option;
  option.kind = HirTypeDeclKind::Enum;
  option.name = "Option";
  option.location = {"<builtin>", 1, 1};
  option.publicDeclaration = true;
  option.builtin = true;
  option.typeParameters = {"T"};
  option.variants = {{"Some", option.location, {typeParameter("T")}},
                     {"None", option.location, {}}};
  typeDeclarations_.emplace(option.name, static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(option));

  HirTypeDeclaration result;
  result.kind = HirTypeDeclKind::Enum;
  result.name = "Result";
  result.location = {"<builtin>", 1, 1};
  result.publicDeclaration = true;
  result.builtin = true;
  result.typeParameters = {"T", "E"};
  result.variants = {{"Ok", result.location, {typeParameter("T")}},
                     {"Err", result.location, {typeParameter("E")}}};
  typeDeclarations_.emplace(result.name, static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(result));

  const Type jsonType{TypeKind::Enum, "std.json.Json"};
  const Type jsonFieldType{TypeKind::Struct, "std.json.JsonField"};
  HirTypeDeclaration stringBuilder;
  stringBuilder.kind = HirTypeDeclKind::Struct;
  stringBuilder.name = "std.string.Builder";
  stringBuilder.location = {"<standard-library>", 1, 1};
  stringBuilder.publicDeclaration = true;
  stringBuilder.builtin = true;
  typeDeclarations_.emplace(stringBuilder.name,
                            static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(stringBuilder));

  HirTypeDeclaration collectionPop;
  collectionPop.kind = HirTypeDeclKind::Struct;
  collectionPop.name = "std.collections.Pop";
  collectionPop.location = {"<standard-library>", 1, 1};
  collectionPop.publicDeclaration = true;
  collectionPop.builtin = true;
  collectionPop.typeParameters = {"T"};
  collectionPop.fields = {{"values", arrayType(typeParameter("T")), collectionPop.location},
                          {"value", typeParameter("T"), collectionPop.location}};
  typeDeclarations_.emplace(collectionPop.name,
                            static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(collectionPop));

  HirTypeDeclaration collectionRemoval;
  collectionRemoval.kind = HirTypeDeclKind::Struct;
  collectionRemoval.name = "std.collections.Removal";
  collectionRemoval.location = {"<standard-library>", 1, 1};
  collectionRemoval.publicDeclaration = true;
  collectionRemoval.builtin = true;
  collectionRemoval.typeParameters = {"T"};
  collectionRemoval.fields = {
      {"values", arrayType(typeParameter("T")), collectionRemoval.location},
      {"value", typeParameter("T"), collectionRemoval.location}};
  typeDeclarations_.emplace(collectionRemoval.name,
                            static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(collectionRemoval));

  auto addCollectionStruct = [&](std::string name, std::vector<std::string> parameters,
                                 std::vector<HirField> fields) {
    HirTypeDeclaration declaration;
    declaration.kind = HirTypeDeclKind::Struct;
    declaration.name = std::move(name);
    declaration.location = {"<standard-library>", 1, 1};
    declaration.publicDeclaration = true;
    declaration.builtin = true;
    declaration.typeParameters = std::move(parameters);
    declaration.fields = std::move(fields);
    for (auto& field : declaration.fields) field.location = declaration.location;
    typeDeclarations_.emplace(
        declaration.name, static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
    hir_.typeDeclarations.push_back(std::move(declaration));
  };
  const Location collectionLocation{"<standard-library>", 1, 1};
  addCollectionStruct(
      "std.collections.Tuple2", {"A", "B"},
      {{"first", typeParameter("A"), collectionLocation},
       {"second", typeParameter("B"), collectionLocation}});
  addCollectionStruct(
      "std.collections.Tuple3", {"A", "B", "C"},
      {{"first", typeParameter("A"), collectionLocation},
       {"second", typeParameter("B"), collectionLocation},
       {"third", typeParameter("C"), collectionLocation}});
  addCollectionStruct(
      "std.collections.Map", {"K", "V"},
      {{"keys", arrayType(typeParameter("K")), collectionLocation},
       {"values", arrayType(typeParameter("V")), collectionLocation}});
  addCollectionStruct(
      "std.collections.Set", {"T"},
      {{"values", arrayType(typeParameter("T")), collectionLocation}});
  addCollectionStruct(
      "std.collections.Queue", {"T"},
      {{"values", arrayType(typeParameter("T")), collectionLocation}});
  addCollectionStruct(
      "std.collections.Stack", {"T"},
      {{"values", arrayType(typeParameter("T")), collectionLocation}});
  addCollectionStruct(
      "std.collections.ByteBuffer", {},
      {{"bytes", arrayType(Type::Char), collectionLocation}});

  HirTypeDeclaration jsonField;
  jsonField.kind = HirTypeDeclKind::Struct;
  jsonField.name = "std.json.JsonField";
  jsonField.location = {"<standard-library>", 1, 1};
  jsonField.publicDeclaration = true;
  jsonField.builtin = true;
  jsonField.fields = {{"key", Type::String, jsonField.location},
                      {"value", jsonType, jsonField.location}};
  typeDeclarations_.emplace(jsonField.name,
                            static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(jsonField));

  HirTypeDeclaration json;
  json.kind = HirTypeDeclKind::Enum;
  json.name = "std.json.Json";
  json.location = {"<standard-library>", 1, 1};
  json.publicDeclaration = true;
  json.builtin = true;
  json.variants = {
      {"std.json.Null", json.location, {}},
      {"std.json.Boolean", json.location, {Type::Bool}},
      {"std.json.Integer", json.location, {Type::Int}},
      {"std.json.Decimal", json.location, {Type::Float}},
      {"std.json.Text", json.location, {Type::String}},
      {"std.json.List", json.location, {arrayType(jsonType)}},
      {"std.json.Object", json.location, {arrayType(jsonFieldType)}},
  };
  typeDeclarations_.emplace(json.name,
                            static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(json));

  for (std::uint32_t declaration = 0; declaration < hir_.typeDeclarations.size(); ++declaration) {
    const auto& type = hir_.typeDeclarations[declaration];
    for (std::uint32_t variant = 0; variant < type.variants.size(); ++variant)
      variants_.emplace(type.variants[variant].name, VariantTarget{declaration, variant});
  }
}

void HirLowerer::registerStandardLibrary() {
  standardFunctions_.clear();
  const Type optionString{TypeKind::Enum, "Option", {Type::String}};
  const Type json{TypeKind::Enum, "std.json.Json"};
  const Type nestedStrings = arrayType(arrayType(Type::String));
  auto result = [](Type success) {
    return Type{TypeKind::Enum, "Result", {std::move(success), Type::String}};
  };
  auto add = [&](std::string name, std::vector<Type> parameters, Type returned,
                 Intrinsic intrinsic, std::vector<std::string> typeParameters = {}) {
    standardFunctions_.emplace(
        std::move(name),
        StandardFunction{std::move(typeParameters), std::move(parameters),
                         std::move(returned), intrinsic});
  };

  add("std.string.byte_length", {Type::String}, Type::Int, Intrinsic::StringByteLength);
  add("std.string.concat", {Type::String, Type::String}, Type::String,
      Intrinsic::StringConcat);
  add("std.string.contains", {Type::String, Type::String}, Type::Bool,
      Intrinsic::StringContains);
  add("std.string.starts_with", {Type::String, Type::String}, Type::Bool,
      Intrinsic::StringStartsWith);
  add("std.string.ends_with", {Type::String, Type::String}, Type::Bool,
      Intrinsic::StringEndsWith);
  add("std.string.trim", {Type::String}, Type::String, Intrinsic::StringTrim);
  add("std.string.split", {Type::String, Type::String}, arrayType(Type::String),
      Intrinsic::StringSplit);
  add("std.string.byte_at", {Type::String, Type::Int}, Type::Char,
      Intrinsic::StringByteAt);
  add("std.string.byte_value_at", {Type::String, Type::Int}, Type::Int,
      Intrinsic::StringByteValueAt);
  add("std.string.slice", {Type::String, Type::Int, Type::Int}, Type::String,
      Intrinsic::StringSlice);
  add("std.string.parse_int", {Type::String}, result(Type::Int),
      Intrinsic::StringParseInt);
  add("std.string.from_int", {Type::Int}, Type::String, Intrinsic::StringFromInt);
  const Type stringBuilderType{TypeKind::Struct, "std.string.Builder"};
  add("std.string.builder", {}, stringBuilderType, Intrinsic::StringBuilderNew);
  add("std.string.builder_append", {stringBuilderType, Type::String}, Type::Unit,
      Intrinsic::StringBuilderAppend);
  add("std.string.builder_finish", {stringBuilderType}, Type::String,
      Intrinsic::StringBuilderFinish);

  const Type t = typeParameter("T");
  const Type collectionPop{TypeKind::Struct, "std.collections.Pop", {t}};
  const Type collectionRemoval{TypeKind::Struct, "std.collections.Removal", {t}};
  const Type k = typeParameter("K");
  const Type v = typeParameter("V");
  const Type mapType{TypeKind::Struct, "std.collections.Map", {k, v}};
  const Type setType{TypeKind::Struct, "std.collections.Set", {t}};
  add("std.collections.length", {arrayType(t)}, Type::Int,
      Intrinsic::CollectionsLength, {"T"});
  add("std.collections.slice_length", {sliceType(t)}, Type::Int,
      Intrinsic::CollectionsLength, {"T"});
  add("std.collections.capacity", {arrayType(t)}, Type::Int,
      Intrinsic::CollectionsCapacity, {"T"});
  add("std.collections.reserve", {arrayType(t), Type::Int}, arrayType(t),
      Intrinsic::CollectionsReserve, {"T"});
  add("std.collections.append", {arrayType(t), t}, arrayType(t),
      Intrinsic::CollectionsAppend, {"T"});
  add("std.collections.pop", {arrayType(t)},
      Type{TypeKind::Enum, "Option", {collectionPop}}, Intrinsic::CollectionsPop, {"T"});
  add("std.collections.insert", {arrayType(t), Type::Int, t}, arrayType(t),
      Intrinsic::CollectionsInsert, {"T"});
  add("std.collections.remove", {arrayType(t), Type::Int}, collectionRemoval,
      Intrinsic::CollectionsRemove, {"T"});
  add("std.collections.clear", {arrayType(t)}, arrayType(t),
      Intrinsic::CollectionsClear, {"T"});
  add("std.collections.map_from_arrays", {arrayType(k), arrayType(v)}, mapType,
      Intrinsic::CollectionsMapFromArrays, {"K", "V"});
  add("std.collections.map_length", {mapType}, Type::Int,
      Intrinsic::CollectionsMapLength, {"K", "V"});
  add("std.collections.map_find", {mapType, k},
      Type{TypeKind::Enum, "Option", {Type::Int}}, Intrinsic::CollectionsMapFind,
      {"K", "V"});
  add("std.collections.map_get", {mapType, k},
      Type{TypeKind::Enum, "Option", {v}}, Intrinsic::CollectionsMapGet, {"K", "V"});
  add("std.collections.map_keys", {mapType}, arrayType(k),
      Intrinsic::CollectionsMapKeys, {"K", "V"});
  add("std.collections.map_values", {mapType}, arrayType(v),
      Intrinsic::CollectionsMapValues, {"K", "V"});
  add("std.collections.set_from_array", {arrayType(t)}, setType,
      Intrinsic::CollectionsSetFromArray, {"T"});
  add("std.collections.set_contains", {setType, t}, Type::Bool,
      Intrinsic::CollectionsSetContains, {"T"});
  add("std.collections.set_values", {setType}, arrayType(t),
      Intrinsic::CollectionsSetValues, {"T"});
  add("std.collections.hash", {t}, Type::Int, Intrinsic::CollectionsHash, {"T"});
  add("std.collections.contains", {arrayType(t), t}, Type::Bool,
      Intrinsic::CollectionsContains, {"T"});
  add("std.collections.find", {arrayType(t), t},
      Type{TypeKind::Enum, "Option", {Type::Int}}, Intrinsic::CollectionsFind, {"T"});
  add("std.collections.filter_equal", {arrayType(t), t}, arrayType(t),
      Intrinsic::CollectionsFilterEqual, {"T"});
  add("std.collections.sort_int", {arrayType(Type::Int)}, arrayType(Type::Int),
      Intrinsic::CollectionsSortInt);
  add("std.collections.sort_float", {arrayType(Type::Float)}, arrayType(Type::Float),
      Intrinsic::CollectionsSortFloat);
  add("std.collections.sort_char", {arrayType(Type::Char)}, arrayType(Type::Char),
      Intrinsic::CollectionsSortChar);
  add("std.collections.sort_string", {arrayType(Type::String)}, arrayType(Type::String),
      Intrinsic::CollectionsSortString);
  add("std.collections.map_hash", {arrayType(t)}, arrayType(Type::Int),
      Intrinsic::CollectionsMapHash, {"T"});
  add("std.collections.fold_sum_int", {arrayType(Type::Int)}, Type::Int,
      Intrinsic::CollectionsFoldSumInt);
  add("std.collections.fold_sum_float", {arrayType(Type::Float)}, Type::Float,
      Intrinsic::CollectionsFoldSumFloat);
  add("std.collections.reverse", {arrayType(t)}, arrayType(t),
      Intrinsic::CollectionsReverse, {"T"});
  add("std.collections.concat", {arrayType(t), arrayType(t)}, arrayType(t),
      Intrinsic::CollectionsConcat, {"T"});
  add("std.collections.join", {arrayType(Type::String), Type::String}, Type::String,
      Intrinsic::CollectionsJoin);

  add("std.file.read_text", {Type::String}, result(Type::String), Intrinsic::FileReadText);
  add("std.file.write_text", {Type::String, Type::String}, result(Type::Bool),
      Intrinsic::FileWriteText);
  add("std.file.append_text", {Type::String, Type::String}, result(Type::Bool),
      Intrinsic::FileAppendText);
  add("std.file.exists", {Type::String}, Type::Bool, Intrinsic::FileExists);
  add("std.file.remove", {Type::String}, result(Type::Bool), Intrinsic::FileRemove);
  add("std.file.list", {Type::String}, result(arrayType(Type::String)), Intrinsic::FileList);
  add("std.file.create_directory", {Type::String}, result(Type::Bool),
      Intrinsic::FileCreateDirectory);

  add("std.path.join", {Type::String, Type::String}, Type::String, Intrinsic::PathJoin);
  add("std.path.basename", {Type::String}, Type::String, Intrinsic::PathBasename);
  add("std.path.extension", {Type::String}, Type::String, Intrinsic::PathExtension);
  add("std.path.normalize", {Type::String}, Type::String, Intrinsic::PathNormalize);

  add("std.json.parse", {Type::String}, result(json), Intrinsic::JsonParse);
  add("std.json.stringify", {json}, Type::String, Intrinsic::JsonStringify);
  add("std.csv.parse", {Type::String}, result(nestedStrings), Intrinsic::CsvParse);
  add("std.csv.encode", {nestedStrings}, Type::String, Intrinsic::CsvEncode);

  add("std.random.seed", {Type::Int}, Type::Unit, Intrinsic::RandomSeed);
  add("std.random.int", {Type::Int, Type::Int}, Type::Int, Intrinsic::RandomInt);
  add("std.random.float", {}, Type::Float, Intrinsic::RandomFloat);

  add("std.process.run", {Type::String, arrayType(Type::String)}, result(Type::Int),
      Intrinsic::ProcessRun);
  add("std.process.arguments", {}, arrayType(Type::String),
      Intrinsic::ProcessArguments);
  add("std.process.executable_path", {}, result(Type::String),
      Intrinsic::ProcessExecutablePath);
  add("std.process.environment", {Type::String}, optionString,
      Intrinsic::ProcessEnvironment);
  add("std.process.working_directory", {}, result(Type::String),
      Intrinsic::ProcessWorkingDirectory);

  add("std.time.unix_milliseconds", {}, Type::Int, Intrinsic::TimeUnixMilliseconds);
  add("std.time.monotonic_milliseconds", {}, Type::Int,
      Intrinsic::TimeMonotonicMilliseconds);
  add("std.time.sleep_milliseconds", {Type::Int}, Type::Unit,
      Intrinsic::TimeSleepMilliseconds);
}

void HirLowerer::registerTypeDeclarations() {
  for (const auto& structure : ast_.structs) {
    if (typeDeclarations_.contains(structure.name)) {
      diagnostics_.error(structure.location, "duplicate type '" + structure.name + "'");
      continue;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(hir_.typeDeclarations.size());
    typeDeclarations_.emplace(structure.name, index);
    hir_.typeDeclarations.push_back({HirTypeDeclKind::Struct, structure.name,
                                     structure.location, structure.publicDeclaration, false,
                                     structure.typeParameters, {}, {}});
  }
  for (const auto& enumeration : ast_.enums) {
    if (typeDeclarations_.contains(enumeration.name)) {
      diagnostics_.error(enumeration.location, "duplicate type '" + enumeration.name + "'");
      continue;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(hir_.typeDeclarations.size());
    typeDeclarations_.emplace(enumeration.name, index);
    hir_.typeDeclarations.push_back({HirTypeDeclKind::Enum, enumeration.name,
                                     enumeration.location, enumeration.publicDeclaration, false,
                                     enumeration.typeParameters, {}, {}});
  }

  auto makeParameters = [&](const std::vector<std::string>& names, const Location& location) {
    Substitutions substitutions;
    for (const auto& name : names) {
      if (!substitutions.emplace(name, typeParameter(name)).second)
        diagnostics_.error(location, "duplicate type parameter '" + name + "'");
    }
    return substitutions;
  };

  for (const auto& structure : ast_.structs) {
    auto found = typeDeclarations_.find(structure.name);
    if (found == typeDeclarations_.end()) continue;
    auto& target = hir_.typeDeclarations[found->second];
    if (target.location.file != structure.location.file ||
        target.location.line != structure.location.line)
      continue;
    const Substitutions parameters = makeParameters(structure.typeParameters, structure.location);
    std::unordered_set<std::string> fields;
    for (const auto& field : structure.fields) {
      if (!fields.insert(field.name).second)
        diagnostics_.error(field.location, "duplicate field '" + field.name + "'");
      target.fields.push_back({field.name, resolveType(field.typeName, field.location, parameters),
                               field.location});
    }
  }

  for (const auto& enumeration : ast_.enums) {
    auto found = typeDeclarations_.find(enumeration.name);
    if (found == typeDeclarations_.end()) continue;
    auto& target = hir_.typeDeclarations[found->second];
    if (target.location.file != enumeration.location.file ||
        target.location.line != enumeration.location.line)
      continue;
    const Substitutions parameters = makeParameters(enumeration.typeParameters,
                                                    enumeration.location);
    std::unordered_set<std::string> localVariants;
    if (enumeration.variants.empty())
      diagnostics_.error(enumeration.location, "enum must declare at least one variant");
    for (const auto& variant : enumeration.variants) {
      if (!localVariants.insert(variant.name).second)
        diagnostics_.error(variant.location, "duplicate variant '" + variant.name + "'");
      if (variants_.contains(variant.name)) {
        diagnostics_.error(variant.location,
                           "variant name '" + variant.name + "' is already declared");
      } else {
        variants_.emplace(variant.name,
                          VariantTarget{found->second,
                                        static_cast<std::uint32_t>(target.variants.size())});
      }
      HirVariant lowered{variant.name, variant.location, {}};
      for (const auto& payload : variant.payloadTypes)
        lowered.payloadTypes.push_back(resolveType(payload, variant.location, parameters));
      target.variants.push_back(std::move(lowered));
    }
  }
}

void HirLowerer::registerTraits() {
  for (const auto& trait : ast_.traits) {
    if (traits_.contains(trait.name)) {
      diagnostics_.error(trait.location, "duplicate trait '" + trait.name + "'");
      continue;
    }
    HirTraitDeclaration lowered;
    lowered.name = trait.name;
    lowered.location = trait.location;
    lowered.publicDeclaration = trait.publicDeclaration;
    std::unordered_set<std::string> methods;
    const Substitutions self{{"Self", typeParameter("Self")}};
    if (trait.methods.empty())
      diagnostics_.error(trait.location, "trait must declare at least one method");
    for (const auto& method : trait.methods) {
      if (!methods.insert(method.name).second)
        diagnostics_.error(method.location, "duplicate trait method '" + method.name + "'");
      HirTraitMethod signature;
      signature.name = method.name;
      for (const auto& parameter : method.parameters)
        signature.parameterTypes.push_back(
            resolveType(parameter.typeName, parameter.location, self));
      signature.result = resolveType(method.returnType, method.location, self);
      if (method.parameters.empty() || method.parameters.front().name != "self" ||
          signature.parameterTypes.front() != typeParameter("Self"))
        diagnostics_.error(method.location,
                           "trait methods require first parameter 'self: Self'");
      lowered.methods.push_back(std::move(signature));
    }
    traits_.emplace(lowered.name,
                    static_cast<std::uint32_t>(hir_.traitDeclarations.size()));
    hir_.traitDeclarations.push_back(std::move(lowered));
  }
}

bool HirLowerer::typeImplementsTrait(const Type& type, const std::string& trait,
                                     const Location& location,
                                     bool diagnoseAmbiguity) const {
  auto matches = [](const Type& pattern, const Type& actual, auto&& matches,
                    std::unordered_map<std::string, Type>& substitutions) -> bool {
    if (pattern.kind == TypeKind::TypeParameter) {
      auto [found, inserted] = substitutions.emplace(pattern.declaration, actual);
      return inserted || found->second == actual;
    }
    if (pattern.kind != actual.kind || pattern.declaration != actual.declaration ||
        pattern.arguments.size() != actual.arguments.size())
      return false;
    for (std::size_t index = 0; index < pattern.arguments.size(); ++index)
      if (!matches(pattern.arguments[index], actual.arguments[index], matches,
                   substitutions))
        return false;
    return true;
  };
  std::unordered_set<std::string> matchingImplementations;
  for (const auto& implementation : traitImplementations_) {
    if (implementation.traitName != trait) continue;
    std::unordered_map<std::string, Type> substitutions;
    if (matches(implementation.ownerPattern, type, matches, substitutions))
      matchingImplementations.insert(typeName(implementation.ownerPattern));
  }
  const std::size_t count = matchingImplementations.size();
  if (count > 1 && diagnoseAmbiguity)
    diagnostics_.error(location, "ambiguous implementations of trait '" + trait +
                                     "' for " + typeName(type));
  return count == 1;
}

std::string HirLowerer::traitMethodTarget(const Type& type, const std::string& member,
                                          const Location& location) const {
  auto matches = [](const Type& pattern, const Type& actual, auto&& matches,
                    std::unordered_map<std::string, Type>& substitutions) -> bool {
    if (pattern.kind == TypeKind::TypeParameter) {
      auto [found, inserted] = substitutions.emplace(pattern.declaration, actual);
      return inserted || found->second == actual;
    }
    if (pattern.kind != actual.kind || pattern.declaration != actual.declaration ||
        pattern.arguments.size() != actual.arguments.size())
      return false;
    for (std::size_t index = 0; index < pattern.arguments.size(); ++index)
      if (!matches(pattern.arguments[index], actual.arguments[index], matches,
                   substitutions))
        return false;
    return true;
  };
  const Function* selected = nullptr;
  for (const auto& implementation : traitImplementations_) {
    if (implementation.member != member) continue;
    std::unordered_map<std::string, Type> substitutions;
    if (!matches(implementation.ownerPattern, type, matches, substitutions)) continue;
    if (selected && selected != implementation.function) {
      diagnostics_.error(location, "ambiguous trait method '" + member + "' for " +
                                       typeName(type));
      return {};
    }
    selected = implementation.function;
  }
  return selected ? selected->name : std::string();
}

std::optional<HirModule> HirLowerer::lower() {
  hir_ = {};
  functions_.clear();
  genericFunctions_.clear();
  associatedConstants_.clear();
  typeDeclarations_.clear();
  traits_.clear();
  traitImplementations_.clear();
  variants_.clear();
  specializations_.clear();
  standardFunctions_.clear();
  pendingSpecializations_.clear();
  pendingLambdas_.clear();
  userSpecializationCount_ = 0;
  functionSymbols_.clear();

  registerBuiltinTypes();
  registerStandardLibrary();
  registerTypeDeclarations();
  registerTraits();

  const SymbolId print = addSymbol(SymbolKind::BuiltinFunction, "print", Type::Unit, false,
                                   {"<builtin>", 1, 1}, {}, Intrinsic::Print);
  functions_.emplace("print", print);

  for (const auto& function : ast_.functions) {
    if (function.associatedConstant) associatedConstants_.insert(function.name);
    if (!function.methodOwner.empty()) {
      Substitutions parameters;
      for (const auto& parameter : function.typeParameters) {
        if (!parameters.emplace(parameter, typeParameter(parameter)).second)
          diagnostics_.error(function.location,
                             "duplicate method type parameter '" + parameter + "'");
      }
      const Type owner = resolveType(function.methodOwner, function.location, parameters);
      if (owner.kind != TypeKind::Struct && owner.kind != TypeKind::Enum)
        diagnostics_.error(function.location,
                           "impl owner must be a struct or enum type");
      const std::size_t memberSeparator = function.name.rfind('.');
      const std::string member = memberSeparator == std::string::npos
                                     ? function.name
                                     : function.name.substr(memberSeparator + 1);
      const std::string expectedName = owner.declaration + "." +
          (function.methodTrait.empty() ? std::string() : function.methodTrait + ".") + member;
      if (function.name != expectedName)
        diagnostics_.error(function.location,
                           "impl owner must be declared in the same module");
      for (std::size_t index = 0; index < function.parameters.size(); ++index) {
        if (function.parameters[index].name != "self") continue;
        if (index != 0) {
          diagnostics_.error(function.parameters[index].location,
                             "'self' must be the first method parameter");
          continue;
        }
        const Type receiver = resolveType(function.parameters[index].typeName,
                                          function.parameters[index].location,
                                          parameters);
        if (receiver != owner)
          diagnostics_.error(function.parameters[index].location,
                             "method receiver must have impl type " + typeName(owner));
      }
      if (!function.methodTrait.empty()) {
        auto trait = traits_.find(function.methodTrait);
        if (trait == traits_.end()) {
          diagnostics_.error(function.location,
                             "unknown trait '" + function.methodTrait + "'");
        } else {
          const auto& declaration = hir_.traitDeclarations[trait->second];
          const HirTraitMethod* required = nullptr;
          for (const auto& method : declaration.methods)
            if (method.name == member) required = &method;
          if (!required) {
            diagnostics_.error(function.location, "trait '" + function.methodTrait +
                                                     "' has no method '" + member + "'");
          } else {
            Substitutions self{{"Self", owner}};
            std::vector<Type> actualParameters;
            for (const auto& parameter : function.parameters)
              actualParameters.push_back(resolveType(parameter.typeName,
                                                     parameter.location, parameters));
            std::vector<Type> requiredParameters;
            for (const auto& parameter : required->parameterTypes)
              requiredParameters.push_back(substitute(parameter, self));
            const Type actualResult = resolveType(function.returnType, function.location,
                                                  parameters);
            if (actualParameters != requiredParameters ||
                actualResult != substitute(required->result, self))
              diagnostics_.error(function.location,
                                 "trait method signature does not match '" +
                                     function.methodTrait + "." + member + "'");
          }
        }
        bool duplicate = false;
        for (const auto& implementation : traitImplementations_)
          if (implementation.traitName == function.methodTrait &&
              implementation.ownerPattern == owner && implementation.member == member)
            duplicate = true;
        if (duplicate)
          diagnostics_.error(function.location, "duplicate trait implementation for " +
                                                   typeName(owner));
        else
          traitImplementations_.push_back(
              {function.methodTrait, owner, member, &function});
      }
    }
    std::unordered_set<std::string> constrained;
    for (const auto& constraint : function.constraints) {
      if (std::find(function.typeParameters.begin(), function.typeParameters.end(),
                    constraint.typeParameter) == function.typeParameters.end())
        diagnostics_.error(constraint.location, "trait constraint names unknown type parameter '" +
                                                    constraint.typeParameter + "'");
      if (!traits_.contains(constraint.traitName))
        diagnostics_.error(constraint.location, "unknown trait '" + constraint.traitName + "'");
      const std::string key = constraint.typeParameter + ":" + constraint.traitName;
      if (!constrained.insert(key).second)
        diagnostics_.error(constraint.location, "duplicate trait constraint '" + key + "'");
    }
    if (functions_.contains(function.name) || genericFunctions_.contains(function.name)) {
      diagnostics_.error(function.location, "duplicate function '" + function.name + "'");
      functionSymbols_.push_back(InvalidSymbol);
      continue;
    }
    if (!function.typeParameters.empty()) {
      genericFunctions_.emplace(function.name, &function);
      functionSymbols_.push_back(InvalidSymbol);
      continue;
    }

    std::vector<Type> parameters;
    for (const auto& parameter : function.parameters)
      parameters.push_back(resolveType(parameter.typeName, parameter.location));
    const Type result = resolveType(function.returnType, function.location);
    const SymbolId symbol = addSymbol(SymbolKind::Function, function.name, result, false,
                                      function.location, parameters);
    functionSymbols_.push_back(symbol);
    functions_.emplace(function.name, symbol);
  }

  for (const auto& implementation : traitImplementations_) {
    const auto trait = traits_.find(implementation.traitName);
    if (trait == traits_.end()) continue;
    const auto& declaration = hir_.traitDeclarations[trait->second];
    for (const auto& required : declaration.methods) {
      bool found = false;
      for (const auto& candidate : traitImplementations_)
        if (candidate.traitName == implementation.traitName &&
            candidate.ownerPattern == implementation.ownerPattern &&
            candidate.member == required.name)
          found = true;
      if (!found)
        diagnostics_.error(implementation.function->location,
                           "implementation of trait '" + implementation.traitName +
                               "' for " + typeName(implementation.ownerPattern) +
                               " is missing method '" + required.name + "'");
    }
  }

  auto main = functions_.find("main");
  if (main == functions_.end()) {
    diagnostics_.error({"<module>", 1, 1}, "program must define fn main() -> Int",
                       DiagnosticCode::ControlFlow);
  } else {
    const auto& signature = hir_.symbol(main->second);
    if (!signature.parameterTypes.empty() || signature.type != Type::Int)
      diagnostics_.error({"<module>", 1, 1},
                         "entry point must have signature fn main() -> Int");
  }

  for (std::size_t index = 0; index < ast_.functions.size(); ++index) {
    if (functionSymbols_[index] != InvalidSymbol)
      hir_.functions.push_back(lowerFunction(ast_.functions[index], functionSymbols_[index]));
  }
  std::size_t specializationIndex = 0;
  std::size_t lambdaIndex = 0;
  while (specializationIndex < pendingSpecializations_.size() ||
         lambdaIndex < pendingLambdas_.size()) {
    if (specializationIndex < pendingSpecializations_.size()) {
      const PendingSpecialization specialization =
          pendingSpecializations_[specializationIndex++];
      hir_.functions.push_back(lowerSpecialization(specialization));
    } else {
      const PendingLambda lambda = pendingLambdas_[lambdaIndex++];
      hir_.functions.push_back(lowerLambda(lambda));
    }
  }

  if (diagnostics_.hasErrors()) return std::nullopt;
  return std::move(hir_);
}

Type HirLowerer::resolveType(const std::string& spelling, const Location& location,
                             const Substitutions& substitutions) {
  const Type parsed = typeFromName(spelling);
  if (parsed == Type::Invalid) {
    diagnostics_.error(location, "invalid type syntax '" + spelling + "'");
    return Type::Invalid;
  }
  return resolveParsedType(parsed, location, substitutions);
}

Type HirLowerer::resolveParsedType(const Type& parsed, const Location& location,
                                   const Substitutions& substitutions) {
  if (parsed.kind == TypeKind::Struct && parsed.arguments.empty()) {
    auto parameter = substitutions.find(parsed.declaration);
    if (parameter != substitutions.end()) return parameter->second;
  }
  if (parsed.kind == TypeKind::Array || parsed.kind == TypeKind::Slice) {
    Type argument = resolveParsedType(parsed.arguments.at(0), location, substitutions);
    if (argument == Type::Unit)
      diagnostics_.error(location, "collections cannot contain Unit values");
    return parsed.kind == TypeKind::Array ? arrayType(argument) : sliceType(argument);
  }
  if (parsed.kind != TypeKind::Struct) return parsed;

  auto found = typeDeclarations_.find(parsed.declaration);
  if (found == typeDeclarations_.end()) {
    diagnostics_.error(location, "unknown type '" + parsed.declaration + "'");
    return Type::Invalid;
  }
  const auto& declaration = hir_.typeDeclarations[found->second];
  if (parsed.arguments.size() != declaration.typeParameters.size()) {
    diagnostics_.error(location, "type '" + parsed.declaration + "' expects " +
                                     std::to_string(declaration.typeParameters.size()) +
                                     " type argument(s)");
    return Type::Invalid;
  }
  std::vector<Type> arguments;
  for (const auto& argument : parsed.arguments)
    arguments.push_back(resolveParsedType(argument, location, substitutions));
  return Type{declaration.kind == HirTypeDeclKind::Struct ? TypeKind::Struct : TypeKind::Enum,
              declaration.name, std::move(arguments)};
}

Type HirLowerer::substitute(const Type& pattern, const Substitutions& substitutions) const {
  if (pattern.kind == TypeKind::TypeParameter) {
    auto found = substitutions.find(pattern.declaration);
    return found == substitutions.end() ? pattern : found->second;
  }
  Type result = pattern;
  for (auto& argument : result.arguments) argument = substitute(argument, substitutions);
  return result;
}

bool HirLowerer::inferTypeArguments(const Type& pattern, const Type& actual,
                                    Substitutions& substitutions,
                                    const Location& location) {
  if (pattern.kind == TypeKind::TypeParameter) {
    auto [found, inserted] = substitutions.emplace(pattern.declaration, actual);
    if (!inserted && found->second != actual) {
      diagnostics_.error(location, "conflicting inferences for type parameter '" +
                                       pattern.declaration + "'");
      return false;
    }
    return true;
  }
  if (pattern.kind != actual.kind || pattern.declaration != actual.declaration ||
      pattern.arguments.size() != actual.arguments.size())
    return false;
  bool valid = true;
  for (std::size_t index = 0; index < pattern.arguments.size(); ++index)
    valid = inferTypeArguments(pattern.arguments[index], actual.arguments[index],
                               substitutions, location) && valid;
  return valid;
}

std::uint32_t HirLowerer::findTypeDeclaration(const Type& type) const {
  auto found = typeDeclarations_.find(type.declaration);
  return found == typeDeclarations_.end() ? static_cast<std::uint32_t>(-1) : found->second;
}

HirFunction HirLowerer::lowerFunction(const Function& function, SymbolId symbol) {
  currentSubstitutions_.clear();
  const HirSymbol signature = hir_.symbol(symbol);
  currentReturnType_ = signature.type;
  scopes_.clear();
  scopes_.emplace_back();
  loopDepth_ = 0;

  HirFunction result;
  result.symbol = symbol;
  result.location = function.location;
  result.result = signature.type;

  std::unordered_set<std::string> names;
  for (std::size_t index = 0; index < function.parameters.size(); ++index) {
    const auto& parameter = function.parameters[index];
    const Type type = signature.parameterTypes[index];
    const SymbolId parameterSymbol = addSymbol(SymbolKind::Parameter, parameter.name, type, false,
                                                parameter.location);
    result.parameters.push_back({parameterSymbol});
    if (!names.insert(parameter.name).second)
      diagnostics_.error(parameter.location, "duplicate parameter '" + parameter.name + "'");
    else
      scopes_.back().emplace(parameter.name, parameterSymbol);
  }

  result.body = lowerBlock(function.body, result.result, false);
  if (result.result != Type::Unit && !definitelyReturns(result.body))
    diagnostics_.error(function.location, "function '" + function.name +
                                           "' may finish without returning " +
                                           typeName(result.result),
                       DiagnosticCode::ControlFlow);
  return result;
}

HirFunction HirLowerer::lowerSpecialization(const PendingSpecialization& specialization) {
  currentSubstitutions_ = specialization.substitutions;
  currentReturnType_ = specialization.result;
  scopes_.clear();
  scopes_.emplace_back();
  loopDepth_ = 0;

  HirFunction result;
  result.symbol = specialization.symbol;
  result.location = specialization.function->location;
  result.result = specialization.result;
  std::unordered_set<std::string> names;
  for (std::size_t index = 0; index < specialization.function->parameters.size(); ++index) {
    const auto& parameter = specialization.function->parameters[index];
    const SymbolId parameterSymbol = addSymbol(SymbolKind::Parameter, parameter.name,
                                                specialization.parameters[index], false,
                                                parameter.location);
    result.parameters.push_back({parameterSymbol});
    if (!names.insert(parameter.name).second)
      diagnostics_.error(parameter.location, "duplicate parameter '" + parameter.name + "'");
    else
      scopes_.back().emplace(parameter.name, parameterSymbol);
  }
  result.body = lowerBlock(specialization.function->body, result.result, false);
  if (result.result != Type::Unit && !definitelyReturns(result.body))
    diagnostics_.error(specialization.function->location,
                       "generic specialization '" + hir_.symbol(result.symbol).name +
                           "' may finish without returning " + typeName(result.result),
                       DiagnosticCode::ControlFlow);
  return result;
}

HirBlock HirLowerer::lowerBlock(const std::vector<std::unique_ptr<Stmt>>& body,
                                Type returnType, bool nested) {
  if (nested) scopes_.emplace_back();
  HirBlock result;
  for (const auto& statement : body)
    result.push_back(lowerStatement(*statement, returnType));
  if (nested) scopes_.pop_back();
  return result;
}

std::unique_ptr<HirStmt> HirLowerer::lowerStatement(const Stmt& statement,
                                                    Type returnType) {
  switch (statement.kind) {
  case StmtKind::Binding: {
    const auto& binding = static_cast<const BindingStmt&>(statement);
    std::optional<Type> declared;
    if (!binding.declaredType.empty())
      declared = resolveType(binding.declaredType, binding.location, currentSubstitutions_);
    auto initializer = lowerExpression(*binding.initializer, declared);
    const Type bindingType = declared.value_or(initializer->type);
    if (declared.has_value() && initializer->type != Type::Invalid &&
        initializer->type != *declared)
      diagnostics_.error(binding.location, "initializer type is " +
                                                typeName(initializer->type) + ", expected " +
                                                typeName(*declared));
    const SymbolId symbol = addSymbol(SymbolKind::Local, binding.name, bindingType,
                                      binding.mutableBinding, binding.location);
    if (scopes_.back().contains(binding.name))
      diagnostics_.error(binding.location,
                         "duplicate binding '" + binding.name + "' in this scope");
    else
      scopes_.back().emplace(binding.name, symbol);
    return std::make_unique<HirBindingStmt>(binding.location, symbol, std::move(initializer));
  }
  case StmtKind::Assignment: {
    const auto& assignment = static_cast<const AssignmentStmt&>(statement);
    const SymbolId target = findVariable(assignment.name);
    const std::optional<Type> expected =
        target == InvalidSymbol ? std::nullopt : std::optional<Type>(hir_.symbol(target).type);
    auto value = lowerExpression(*assignment.value, expected);
    if (target == InvalidSymbol) {
      diagnostics_.error(assignment.location,
                         "cannot assign to undefined name '" + assignment.name + "'");
    } else {
      const auto& symbol = hir_.symbol(target);
      if (!symbol.mutableBinding)
        diagnostics_.error(assignment.location,
                           "cannot assign to immutable binding '" + assignment.name + "'");
      else if (value->type != Type::Invalid && value->type != symbol.type)
        diagnostics_.error(assignment.location, "assignment type is " +
                                                    typeName(value->type) + ", expected " +
                                                    typeName(symbol.type));
    }
    return std::make_unique<HirAssignmentStmt>(assignment.location, target, std::move(value));
  }
  case StmtKind::IndexAssignment: {
    const auto& assignment = static_cast<const IndexAssignmentStmt&>(statement);
    const SymbolId target = findVariable(assignment.name);
    Type elementType = Type::Invalid;
    if (target == InvalidSymbol) {
      diagnostics_.error(assignment.location,
                         "cannot assign through undefined name '" + assignment.name + "'");
    } else {
      const auto& symbol = hir_.symbol(target);
      if (!symbol.mutableBinding)
        diagnostics_.error(assignment.location,
                           "cannot mutate immutable binding '" + assignment.name + "'");
      if (!isArrayType(symbol.type))
        diagnostics_.error(assignment.location,
                           "indexed assignment requires a mutable Array binding");
      else
        elementType = collectionElementType(symbol.type);
    }
    auto index = lowerExpression(*assignment.index, Type::Int);
    auto value = lowerExpression(*assignment.value,
                                 elementType == Type::Invalid
                                     ? std::nullopt
                                     : std::optional<Type>(elementType));
    if (index->type != Type::Invalid && index->type != Type::Int)
      diagnostics_.error(assignment.index->location,
                         "Array assignment index must have type Int");
    if (elementType != Type::Invalid && value->type != Type::Invalid &&
        value->type != elementType)
      diagnostics_.error(assignment.value->location,
                         "Array assignment value is " + typeName(value->type) +
                             ", expected " + typeName(elementType));
    return std::make_unique<HirIndexAssignmentStmt>(
        assignment.location, target, std::move(index), std::move(value));
  }
  case StmtKind::Return: {
    const auto& returned = static_cast<const ReturnStmt&>(statement);
    auto value = returned.value ? lowerExpression(*returned.value, returnType) : nullptr;
    const Type actual = value ? value->type : Type::Unit;
    if (actual != Type::Invalid && returnType != Type::Invalid && actual != returnType)
      diagnostics_.error(returned.location, "return type is " + typeName(actual) +
                                                ", expected " + typeName(returnType));
    return std::make_unique<HirReturnStmt>(returned.location, std::move(value));
  }
  case StmtKind::Expression: {
    const auto& expression = static_cast<const ExprStmt&>(statement);
    return std::make_unique<HirExprStmt>(expression.location,
                                         lowerExpression(*expression.expression));
  }
  case StmtKind::If: {
    const auto& branch = static_cast<const IfStmt&>(statement);
    auto condition = lowerExpression(*branch.condition, Type::Bool);
    if (condition->type != Type::Invalid && condition->type != Type::Bool)
      diagnostics_.error(branch.condition->location, "if condition must have type Bool");
    auto thenBody = lowerBlock(branch.thenBody, returnType, true);
    auto elseBody = lowerBlock(branch.elseBody, returnType, true);
    return std::make_unique<HirIfStmt>(branch.location, std::move(condition),
                                       std::move(thenBody), std::move(elseBody));
  }
  case StmtKind::While: {
    const auto& loop = static_cast<const WhileStmt&>(statement);
    auto condition = lowerExpression(*loop.condition, Type::Bool);
    if (condition->type != Type::Invalid && condition->type != Type::Bool)
      diagnostics_.error(loop.condition->location, "while condition must have type Bool");
    ++loopDepth_;
    auto body = lowerBlock(loop.body, returnType, true);
    --loopDepth_;
    return std::make_unique<HirWhileStmt>(loop.location, std::move(condition), std::move(body));
  }
  case StmtKind::For: {
    const auto& loop = static_cast<const ForStmt&>(statement);
    if (!loop.rangeLoop) {
      auto source = lowerExpression(*loop.start);
      auto method = [&](const Type& receiver, const std::string& member) {
        std::string target = libraryMethodFunction(receiver, member);
        if (target.empty() &&
            (receiver.kind == TypeKind::Struct || receiver.kind == TypeKind::Enum))
          target = receiver.declaration + "." + member;
        if (!target.empty() && !functions_.contains(target) &&
            !genericFunctions_.contains(target) && !standardFunctions_.contains(target))
          target.clear();
        if (target.empty()) target = traitMethodTarget(receiver, member, loop.location);
        return target;
      };
      std::string iteratorTarget = method(source->type, "iterator");
      if (iteratorTarget.empty())
        diagnostics_.error(loop.location,
                           "for source type " + typeName(source->type) +
                               " has no iterator() method");
      std::vector<std::unique_ptr<HirExpr>> iteratorArguments;
      iteratorArguments.push_back(std::move(source));
      auto iterator = lowerResolvedCall(iteratorTarget, loop.location,
                                        std::move(iteratorArguments));
      const Type cursorType = iterator->type;
      const SymbolId cursor = addSymbol(SymbolKind::Local,
                                        "$iterator." + std::to_string(hir_.symbols.size()),
                                        cursorType, true, loop.location);
      auto cursorValue = [&]() {
        return std::make_unique<HirNameExpr>(loop.location, cursorType, cursor);
      };
      auto callCursor = [&](const std::string& member) {
        const std::string target = method(cursorType, member);
        if (target.empty())
          diagnostics_.error(loop.location, "iterator type " + typeName(cursorType) +
                                                 " has no " + member + "() method");
        std::vector<std::unique_ptr<HirExpr>> arguments;
        arguments.push_back(cursorValue());
        return lowerResolvedCall(target, loop.location, std::move(arguments));
      };
      auto condition = callCursor("has_next");
      auto value = callCursor("value");
      auto advance = callCursor("advance");
      if (condition->type != Type::Invalid && condition->type != Type::Bool)
        diagnostics_.error(loop.location, "iterator has_next() must return Bool");
      if (advance->type != Type::Invalid && advance->type != cursorType)
        diagnostics_.error(loop.location, "iterator advance() must return " +
                                             typeName(cursorType));
      scopes_.emplace_back();
      const SymbolId variable = addSymbol(SymbolKind::LoopVariable, loop.name,
                                          value->type, false, loop.location);
      scopes_.back().emplace(loop.name, variable);
      ++loopDepth_;
      auto body = lowerBlock(loop.body, returnType, false);
      --loopDepth_;
      scopes_.pop_back();
      return std::make_unique<HirForEachStmt>(
          loop.location, cursor, variable, std::move(iterator),
          std::move(condition), std::move(value), std::move(advance),
          std::move(body));
    }
    auto start = lowerExpression(*loop.start, Type::Int);
    auto end = lowerExpression(*loop.end, Type::Int);
    if (start->type != Type::Invalid && start->type != Type::Int)
      diagnostics_.error(loop.start->location, "range start must have type Int");
    if (end->type != Type::Invalid && end->type != Type::Int)
      diagnostics_.error(loop.end->location, "range end must have type Int");
    scopes_.emplace_back();
    const SymbolId variable = addSymbol(SymbolKind::LoopVariable, loop.name, Type::Int, false,
                                        loop.location);
    scopes_.back().emplace(loop.name, variable);
    ++loopDepth_;
    auto body = lowerBlock(loop.body, returnType, false);
    --loopDepth_;
    scopes_.pop_back();
    return std::make_unique<HirForStmt>(loop.location, variable, std::move(start),
                                        std::move(end), std::move(body));
  }
  case StmtKind::Break:
  case StmtKind::Continue: {
    if (loopDepth_ == 0)
      diagnostics_.error(statement.location, statement.kind == StmtKind::Break
                                                 ? "'break' is only valid inside a loop"
                                                 : "'continue' is only valid inside a loop",
                         DiagnosticCode::ControlFlow);
    return std::make_unique<HirLoopControlStmt>(
        statement.kind == StmtKind::Break ? HirStmtKind::Break : HirStmtKind::Continue,
        statement.location);
  }
  case StmtKind::Match: {
    const auto& match = static_cast<const MatchStmt&>(statement);
    auto value = lowerExpression(*match.value);
    const std::uint32_t declarationIndex = findTypeDeclaration(value->type);
    const HirTypeDeclaration* declaration = nullptr;
    if (value->type.kind != TypeKind::Enum ||
        declarationIndex == static_cast<std::uint32_t>(-1)) {
      diagnostics_.error(match.value->location, "match requires an enum value",
                         DiagnosticCode::PatternMatch);
    } else {
      declaration = &hir_.typeDeclarations[declarationIndex];
    }

    Substitutions substitutions;
    if (declaration)
      for (std::size_t index = 0; index < declaration->typeParameters.size(); ++index)
        substitutions.emplace(declaration->typeParameters[index], value->type.arguments[index]);
    std::unordered_set<std::uint32_t> seen;
    bool wildcardSeen = false;
    std::vector<HirMatchCase> cases;
    for (std::size_t caseIndex = 0; caseIndex < match.cases.size(); ++caseIndex) {
      const auto& sourceCase = match.cases[caseIndex];
      scopes_.emplace_back();
      HirMatchCase lowered{sourceCase.pattern.location, std::nullopt, {}, {}};
      if (sourceCase.pattern.wildcard) {
        if (wildcardSeen)
          diagnostics_.error(sourceCase.pattern.location, "duplicate wildcard match case");
        wildcardSeen = true;
        if (caseIndex + 1 != match.cases.size())
          diagnostics_.error(sourceCase.pattern.location,
                             "wildcard match case must be last");
        if (!sourceCase.pattern.bindings.empty())
          diagnostics_.error(sourceCase.pattern.location,
                             "wildcard pattern cannot bind payload values");
      } else if (declaration) {
        std::optional<std::uint32_t> tag;
        for (std::uint32_t index = 0; index < declaration->variants.size(); ++index)
          if (declaration->variants[index].name == sourceCase.pattern.variant) tag = index;
        if (!tag.has_value()) {
          diagnostics_.error(sourceCase.pattern.location, "enum '" + declaration->name +
                                                              "' has no variant '" +
                                                              sourceCase.pattern.variant + "'");
        } else {
          lowered.tag = tag;
          if (!seen.insert(*tag).second)
            diagnostics_.error(sourceCase.pattern.location,
                               "duplicate match case for '" + sourceCase.pattern.variant + "'");
          const auto& payloads = declaration->variants[*tag].payloadTypes;
          if (payloads.size() != sourceCase.pattern.bindings.size())
            diagnostics_.error(sourceCase.pattern.location, "variant '" +
                                                            sourceCase.pattern.variant +
                                                            "' binds " +
                                                            std::to_string(payloads.size()) +
                                                            " value(s)");
          std::unordered_set<std::string> bindingNames;
          for (std::size_t index = 0;
               index < payloads.size() && index < sourceCase.pattern.bindings.size(); ++index) {
            const std::string& name = sourceCase.pattern.bindings[index];
            if (!bindingNames.insert(name).second)
              diagnostics_.error(sourceCase.pattern.location,
                                 "duplicate pattern binding '" + name + "'");
            const SymbolId symbol = addSymbol(SymbolKind::PatternBinding, name,
                                               substitute(payloads[index], substitutions), false,
                                               sourceCase.pattern.location);
            scopes_.back().emplace(name, symbol);
            lowered.bindings.push_back(symbol);
          }
        }
      }
      lowered.body = lowerBlock(sourceCase.body, returnType, false);
      scopes_.pop_back();
      cases.push_back(std::move(lowered));
    }
    if (declaration && !wildcardSeen && seen.size() != declaration->variants.size()) {
      std::string missing;
      for (std::uint32_t index = 0; index < declaration->variants.size(); ++index) {
        if (seen.contains(index)) continue;
        if (!missing.empty()) missing += ", ";
        missing += declaration->variants[index].name;
      }
      diagnostics_.error(match.location, "non-exhaustive match; missing " + missing,
                         DiagnosticCode::PatternMatch);
    }
    return std::make_unique<HirMatchStmt>(match.location, std::move(value), declarationIndex,
                                          std::move(cases));
  }
  }
  return std::make_unique<HirLoopControlStmt>(HirStmtKind::Break, statement.location);
}

SymbolId HirLowerer::specializeFunction(
    const Function& function, const std::vector<std::unique_ptr<HirExpr>>& arguments,
    const Location& location) {
  Substitutions parameterMarkers;
  for (const auto& parameter : function.typeParameters)
    parameterMarkers.emplace(parameter, typeParameter(parameter));
  if (arguments.size() != function.parameters.size()) {
    diagnostics_.error(location, "generic function '" + function.name + "' expects " +
                                     std::to_string(function.parameters.size()) + " argument(s)",
                       DiagnosticCode::Arity);
    return InvalidSymbol;
  }

  std::vector<Type> patterns;
  Substitutions inferred;
  for (std::size_t index = 0; index < function.parameters.size(); ++index) {
    Type pattern = resolveType(function.parameters[index].typeName,
                               function.parameters[index].location, parameterMarkers);
    patterns.push_back(pattern);
    if (!inferTypeArguments(pattern, arguments[index]->type, inferred,
                            arguments[index]->location))
      diagnostics_.error(arguments[index]->location, "argument type " +
                                                       typeName(arguments[index]->type) +
                                                       " does not match generic parameter " +
                                                       typeName(pattern));
  }
  for (const auto& parameter : function.typeParameters) {
    if (!inferred.contains(parameter)) {
      diagnostics_.error(location, "cannot infer type argument '" + parameter +
                                       "' for generic function '" + function.name + "'");
      inferred.emplace(parameter, Type::Invalid);
    }
  }
  for (const auto& constraint : function.constraints) {
    auto inferredType = inferred.find(constraint.typeParameter);
    if (inferredType != inferred.end() && inferredType->second != Type::Invalid &&
        !typeImplementsTrait(inferredType->second, constraint.traitName, location))
      diagnostics_.error(location, "type " + typeName(inferredType->second) +
                                       " does not implement trait '" +
                                       constraint.traitName + "'");
  }

  std::string key = function.name + "[";
  for (std::size_t index = 0; index < function.typeParameters.size(); ++index) {
    if (index) key += ",";
    key += typeName(inferred.at(function.typeParameters[index]));
  }
  key += ']';
  auto existing = specializations_.find(key);
  if (existing != specializations_.end()) return existing->second;

  constexpr std::size_t MaxUserSpecializations = 4096;
  if (userSpecializationCount_ >= MaxUserSpecializations) {
    diagnostics_.error(location,
                       "generic specialization limit of 4096 exceeded");
    return InvalidSymbol;
  }
  ++userSpecializationCount_;

  std::vector<Type> parameters;
  for (const auto& pattern : patterns) parameters.push_back(substitute(pattern, inferred));
  Type resultPattern = resolveType(function.returnType, function.location, parameterMarkers);
  Type result = substitute(resultPattern, inferred);
  const SymbolId symbol = addSymbol(SymbolKind::Function, key, result, false,
                                    function.location, parameters);
  specializations_.emplace(key, symbol);
  pendingSpecializations_.push_back({&function, symbol, std::move(inferred),
                                     std::move(parameters), std::move(result)});
  return symbol;
}

void HirLowerer::collectLambdaCaptures(
    const Expr& expression, const std::unordered_set<std::string>& parameters,
    std::vector<LambdaCapture>& captures) const {
  auto addName = [&](const LiteralExpr& name) {
    if (parameters.contains(name.value)) return;
    const SymbolId symbol = findVariable(name.value);
    if (symbol == InvalidSymbol) return;
    for (const auto& capture : captures)
      if (capture.source == symbol) return;
    captures.push_back({name.value, symbol,
                        static_cast<std::uint32_t>(captures.size())});
  };
  switch (expression.kind) {
  case ExprKind::Name: addName(static_cast<const LiteralExpr&>(expression)); break;
  case ExprKind::Unary:
    collectLambdaCaptures(*static_cast<const UnaryExpr&>(expression).operand,
                          parameters, captures); break;
  case ExprKind::Binary: {
    const auto& binary = static_cast<const BinaryExpr&>(expression);
    collectLambdaCaptures(*binary.left, parameters, captures);
    collectLambdaCaptures(*binary.right, parameters, captures);
    break;
  }
  case ExprKind::Call: {
    const auto& call = static_cast<const CallExpr&>(expression);
    collectLambdaCaptures(*call.callee, parameters, captures);
    for (const auto& argument : call.arguments)
      collectLambdaCaptures(*argument, parameters, captures);
    break;
  }
  case ExprKind::Array:
    for (const auto& element : static_cast<const ArrayExpr&>(expression).elements)
      collectLambdaCaptures(*element, parameters, captures);
    break;
  case ExprKind::Index: {
    const auto& index = static_cast<const IndexExpr&>(expression);
    collectLambdaCaptures(*index.collection, parameters, captures);
    collectLambdaCaptures(*index.index, parameters, captures);
    break;
  }
  case ExprKind::Slice: {
    const auto& slice = static_cast<const SliceExpr&>(expression);
    collectLambdaCaptures(*slice.collection, parameters, captures);
    collectLambdaCaptures(*slice.start, parameters, captures);
    collectLambdaCaptures(*slice.end, parameters, captures);
    break;
  }
  case ExprKind::Field:
    collectLambdaCaptures(*static_cast<const FieldExpr&>(expression).value,
                          parameters, captures); break;
  case ExprKind::Propagate:
    collectLambdaCaptures(*static_cast<const PropagateExpr&>(expression).value,
                          parameters, captures); break;
  case ExprKind::Lambda: break;
  default: break;
  }
}

HirFunction HirLowerer::lowerLambda(const PendingLambda& pending) {
  currentSubstitutions_.clear();
  currentReturnType_ = hir_.symbol(pending.symbol).type;
  scopes_.clear();
  scopes_.emplace_back();
  activeCaptures_.clear();
  loopDepth_ = 0;

  HirFunction result;
  result.symbol = pending.symbol;
  result.location = pending.lambda->location;
  result.result = currentReturnType_;
  const SymbolId closure = addSymbol(SymbolKind::Parameter, "$closure",
                                     pending.closureType, false,
                                     pending.lambda->location);
  scopes_.back().emplace("$closure", closure);
  result.parameters.push_back({closure});
  for (const auto& capture : pending.captures)
    activeCaptures_.emplace(
        capture.name,
        ActiveCapture{closure, capture.field, hir_.symbol(capture.source).type});
  for (const auto& parameter : pending.lambda->parameters) {
    const Type type = resolveType(parameter.typeName, parameter.location);
    const SymbolId symbol = addSymbol(SymbolKind::Parameter, parameter.name, type,
                                      false, parameter.location);
    scopes_.back().emplace(parameter.name, symbol);
    result.parameters.push_back({symbol});
  }
  auto value = lowerExpression(*pending.lambda->body, currentReturnType_);
  if (value->type != Type::Invalid && value->type != currentReturnType_)
    diagnostics_.error(pending.lambda->body->location,
                       "lambda result is " + typeName(value->type) + ", expected " +
                           typeName(currentReturnType_));
  result.body.push_back(std::make_unique<HirReturnStmt>(pending.lambda->location,
                                                        std::move(value)));
  activeCaptures_.clear();
  return result;
}

std::unique_ptr<HirExpr> HirLowerer::lowerResolvedCall(
    const std::string& name, const Location& location,
    std::vector<std::unique_ptr<HirExpr>> arguments) {
  if (auto standard = standardFunctions_.find(name);
      standard != standardFunctions_.end()) {
    const StandardFunction& definition = standard->second;
    if (arguments.size() != definition.parameterTypes.size())
      diagnostics_.error(location, "standard function '" + name + "' expects " +
                                       std::to_string(definition.parameterTypes.size()) +
                                       " argument(s)", DiagnosticCode::Arity);
    Substitutions inferred;
    for (std::size_t index = 0;
         index < arguments.size() && index < definition.parameterTypes.size(); ++index) {
      if (!inferTypeArguments(definition.parameterTypes[index], arguments[index]->type,
                              inferred, arguments[index]->location))
        diagnostics_.error(arguments[index]->location, "argument type is " +
                                                        typeName(arguments[index]->type) +
                                                        ", expected " +
                                                        typeName(definition.parameterTypes[index]));
    }
    std::string key = name;
    if (!definition.typeParameters.empty()) key += '[';
    for (std::size_t index = 0; index < definition.typeParameters.size(); ++index) {
      const auto& parameter = definition.typeParameters[index];
      auto found = inferred.find(parameter);
      if (found == inferred.end()) {
        diagnostics_.error(location, "cannot infer standard-library type argument '" +
                                         parameter + "'");
        inferred.emplace(parameter, Type::Invalid);
        found = inferred.find(parameter);
      }
      if (index) key += ',';
      key += typeName(found->second);
    }
    if (!definition.typeParameters.empty()) key += ']';
    std::vector<Type> parameterTypes;
    for (const auto& parameter : definition.parameterTypes)
      parameterTypes.push_back(substitute(parameter, inferred));
    const Type result = substitute(definition.result, inferred);
    if ((definition.intrinsic == Intrinsic::CollectionsMapFind ||
         definition.intrinsic == Intrinsic::CollectionsMapGet ||
         definition.intrinsic == Intrinsic::CollectionsMapKeys ||
         definition.intrinsic == Intrinsic::CollectionsMapValues ||
         definition.intrinsic == Intrinsic::CollectionsMapLength) &&
        inferred.contains("K") && !isHashableKey(inferred.at("K")))
      diagnostics_.error(location, "Map keys must be Int, Bool, Char, or String");
    if ((definition.intrinsic == Intrinsic::CollectionsSetContains ||
         definition.intrinsic == Intrinsic::CollectionsSetValues) &&
        inferred.contains("T") && !isHashableKey(inferred.at("T")))
      diagnostics_.error(location,
                         "Set elements and hash values must be Int, Bool, Char, or String");
    if ((definition.intrinsic == Intrinsic::CollectionsContains ||
         definition.intrinsic == Intrinsic::CollectionsFind ||
         definition.intrinsic == Intrinsic::CollectionsFilterEqual) &&
        inferred.contains("T") && !isCollectionComparable(inferred.at("T")))
      diagnostics_.error(location,
                         "collection equality requires Int, Float, Bool, Char, or String");
    SymbolId callee = InvalidSymbol;
    if (auto found = specializations_.find(key); found != specializations_.end()) {
      callee = found->second;
    } else {
      callee = addSymbol(SymbolKind::BuiltinFunction, key, result, false,
                         {"<standard-library>", 1, 1}, parameterTypes,
                         definition.intrinsic);
      specializations_.emplace(key, callee);
    }
    for (std::size_t index = 0;
         index < arguments.size() && index < parameterTypes.size(); ++index)
      if (arguments[index]->type != Type::Invalid &&
          arguments[index]->type != parameterTypes[index])
        diagnostics_.error(arguments[index]->location, "argument type is " +
                                                        typeName(arguments[index]->type) +
                                                        ", expected " +
                                                        typeName(parameterTypes[index]));
    return std::make_unique<HirCallExpr>(location, result, callee,
                                         std::move(arguments));
  }
  if (auto generic = genericFunctions_.find(name); generic != genericFunctions_.end()) {
    const SymbolId callee = specializeFunction(*generic->second, arguments, location);
    const Type result = callee == InvalidSymbol ? Type::Invalid : hir_.symbol(callee).type;
    return std::make_unique<HirCallExpr>(location, result, callee,
                                         std::move(arguments));
  }

  auto found = functions_.find(name);
  if (found == functions_.end()) {
    diagnostics_.error(location, "unknown method '" + name + "'", DiagnosticCode::Name);
    return std::make_unique<HirCallExpr>(location, Type::Invalid, InvalidSymbol,
                                         std::move(arguments));
  }
  const SymbolId callee = found->second;
  const HirSymbol signature = hir_.symbol(callee);
  if (arguments.size() != signature.parameterTypes.size())
    diagnostics_.error(location, "method '" + name + "' expects " +
                                     std::to_string(signature.parameterTypes.size()) +
                                     " argument(s)", DiagnosticCode::Arity);
  for (std::size_t index = 0;
       index < arguments.size() && index < signature.parameterTypes.size(); ++index)
    if (arguments[index]->type != Type::Invalid &&
        arguments[index]->type != signature.parameterTypes[index])
      diagnostics_.error(arguments[index]->location, "argument type is " +
                                                      typeName(arguments[index]->type) +
                                                      ", expected " +
                                                      typeName(signature.parameterTypes[index]));
  return std::make_unique<HirCallExpr>(location, signature.type, callee,
                                       std::move(arguments));
}

std::unique_ptr<HirExpr> HirLowerer::lowerExpression(const Expr& expression,
                                                     std::optional<Type> expected) {
  switch (expression.kind) {
  case ExprKind::Integer: {
    const auto& literal = static_cast<const LiteralExpr&>(expression).value;
    std::int64_t parsed = 0;
    const auto result = std::from_chars(literal.data(), literal.data() + literal.size(), parsed);
    if (result.ec == std::errc::result_out_of_range)
      diagnostics_.error(expression.location, "Int literal is outside the signed 64-bit range");
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Int, literal);
  }
  case ExprKind::Float:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Float,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Character:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Char,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::String:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::String,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Bool:
    return std::make_unique<HirLiteralExpr>(expression.location, Type::Bool,
                                            static_cast<const LiteralExpr&>(expression).value);
  case ExprKind::Name: {
    const auto& name = static_cast<const LiteralExpr&>(expression).value;
    if (auto capture = activeCaptures_.find(name); capture != activeCaptures_.end()) {
      auto closure = std::make_unique<HirNameExpr>(
          expression.location, hir_.symbol(capture->second.closure).type,
          capture->second.closure);
      return std::make_unique<HirFieldExpr>(expression.location,
                                            capture->second.type,
                                            std::move(closure),
                                            capture->second.field);
    }
    const SymbolId symbol = findVariable(name);
    if (symbol == InvalidSymbol) {
      diagnostics_.error(expression.location, "undefined name '" + name + "'",
                         DiagnosticCode::Name);
      return std::make_unique<HirNameExpr>(expression.location, Type::Invalid, symbol);
    }
    return std::make_unique<HirNameExpr>(expression.location, hir_.symbol(symbol).type, symbol);
  }
  case ExprKind::Unary: {
    const auto& unary = static_cast<const UnaryExpr&>(expression);
    auto operand = lowerExpression(*unary.operand);
    Type result = operand->type;
    if (unary.op == TokenKind::KwNot) {
      if (operand->type != Type::Bool && operand->type != Type::Invalid)
        diagnostics_.error(expression.location, "'not' requires a Bool operand");
      result = operand->type == Type::Invalid ? Type::Invalid : Type::Bool;
    } else if (operand->type != Type::Int && operand->type != Type::Float &&
               operand->type != Type::Invalid) {
      diagnostics_.error(expression.location, "unary '-' requires Int or Float");
    }
    return std::make_unique<HirUnaryExpr>(expression.location, result, unary.op,
                                          std::move(operand));
  }
  case ExprKind::Binary: {
    const auto& binary = static_cast<const BinaryExpr&>(expression);
    auto left = lowerExpression(*binary.left);
    auto right = lowerExpression(*binary.right, left->type);
    Type result = Type::Invalid;
    if (left->type != Type::Invalid && right->type != Type::Invalid) {
      if (left->type != right->type) {
        diagnostics_.error(expression.location, "operator operands have different types");
      } else {
        switch (binary.op) {
        case TokenKind::KwAnd:
        case TokenKind::KwOr:
          if (left->type != Type::Bool)
            diagnostics_.error(expression.location, "logical operators require Bool operands");
          else result = Type::Bool;
          break;
        case TokenKind::EqualEqual:
        case TokenKind::BangEqual:
          if (isCollectionType(left->type) || isAggregateType(left->type))
            diagnostics_.error(expression.location,
                               "aggregate equality is not available; match or compare fields");
          else result = Type::Bool;
          break;
        case TokenKind::Less:
        case TokenKind::LessEqual:
        case TokenKind::Greater:
        case TokenKind::GreaterEqual:
          if (left->type != Type::Int && left->type != Type::Float)
            diagnostics_.error(expression.location,
                               "ordering operators require Int or Float operands");
          else result = Type::Bool;
          break;
        case TokenKind::Plus:
        case TokenKind::Minus:
        case TokenKind::Star:
        case TokenKind::Slash:
          if (left->type != Type::Int && left->type != Type::Float)
            diagnostics_.error(expression.location,
                               "arithmetic operators require Int or Float operands");
          else result = left->type;
          break;
        default: break;
        }
      }
    }
    return std::make_unique<HirBinaryExpr>(expression.location, result, std::move(left),
                                           binary.op, std::move(right));
  }
  case ExprKind::Call: {
    const auto& call = static_cast<const CallExpr&>(expression);
    if (call.callee->kind == ExprKind::Name) {
      const std::string& name = static_cast<const LiteralExpr&>(*call.callee).value;
      const SymbolId variable = findVariable(name);
      if (variable != InvalidSymbol) {
        const Type closureType = hir_.symbol(variable).type;
        if (closureType.kind == TypeKind::Struct &&
            closureType.declaration.rfind("$closure.", 0) == 0) {
          std::vector<std::unique_ptr<HirExpr>> arguments;
          arguments.push_back(std::make_unique<HirNameExpr>(
              call.callee->location, closureType, variable));
          for (const auto& argument : call.arguments)
            arguments.push_back(lowerExpression(*argument));
          return lowerResolvedCall(closureType.declaration + ".call",
                                   expression.location, std::move(arguments));
        }
      }
    }
    if (call.callee->kind == ExprKind::Lambda) {
      auto closure = lowerExpression(*call.callee);
      const Type closureType = closure->type;
      std::vector<std::unique_ptr<HirExpr>> arguments;
      arguments.push_back(std::move(closure));
      for (const auto& argument : call.arguments)
        arguments.push_back(lowerExpression(*argument));
      return lowerResolvedCall(closureType.declaration + ".call",
                               expression.location, std::move(arguments));
    }
    if (call.callee->kind == ExprKind::Field) {
      const auto& field = static_cast<const FieldExpr&>(*call.callee);
      if (field.value->kind == ExprKind::Name) {
        const std::string owner = static_cast<const LiteralExpr&>(*field.value).value;
        const std::string associated = owner + "." + field.field;
        if (typeDeclarations_.contains(owner) || owner == "String") {
          std::vector<std::unique_ptr<HirExpr>> arguments;
          for (const auto& argument : call.arguments)
            arguments.push_back(lowerExpression(*argument));
          return lowerResolvedCall(associatedLibraryFunction(associated),
                                   expression.location, std::move(arguments));
        }
      }
      std::vector<std::unique_ptr<HirExpr>> arguments;
      auto receiver = lowerExpression(*field.value);
      const Type receiverType = receiver->type;
      arguments.push_back(std::move(receiver));
      for (const auto& argument : call.arguments)
        arguments.push_back(lowerExpression(*argument));
      std::string target = libraryMethodFunction(receiverType, field.field);
      if (target.empty() &&
          (receiverType.kind == TypeKind::Struct || receiverType.kind == TypeKind::Enum))
        target = receiverType.declaration + "." + field.field;
      if (!target.empty() && !functions_.contains(target) &&
          !genericFunctions_.contains(target) && !standardFunctions_.contains(target))
        target.clear();
      if (target.empty() &&
          (receiverType.kind == TypeKind::Struct || receiverType.kind == TypeKind::Enum))
        target = traitMethodTarget(receiverType, field.field, expression.location);
      if (target.empty()) target = typeName(receiverType) + "." + field.field;
      return lowerResolvedCall(target, expression.location, std::move(arguments));
    }
    if (call.callee->kind != ExprKind::Name) {
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (const auto& argument : call.arguments) arguments.push_back(lowerExpression(*argument));
      diagnostics_.error(expression.location,
                         "call target must be a function or constructor name",
                         DiagnosticCode::Name);
      return std::make_unique<HirCallExpr>(expression.location, Type::Invalid, InvalidSymbol,
                                           std::move(arguments));
    }
    const std::string name = associatedLibraryFunction(
        static_cast<const LiteralExpr&>(*call.callee).value);

    std::optional<std::uint32_t> aggregateDeclaration;
    std::uint32_t tag = 0;
    bool structConstructor = false;
    auto typeTarget = typeDeclarations_.find(name);
    if (typeTarget != typeDeclarations_.end() &&
        hir_.typeDeclarations[typeTarget->second].kind == HirTypeDeclKind::Struct) {
      aggregateDeclaration = typeTarget->second;
      structConstructor = true;
    } else {
      auto variant = variants_.find(name);
      if (variant != variants_.end()) {
        aggregateDeclaration = variant->second.declaration;
        tag = variant->second.variant;
      }
    }

    if (aggregateDeclaration.has_value()) {
      const auto& declaration = hir_.typeDeclarations[*aggregateDeclaration];
      std::vector<Type> patterns;
      if (structConstructor) {
        for (const auto& field : declaration.fields) patterns.push_back(field.type);
      } else {
        patterns = declaration.variants[tag].payloadTypes;
      }
      Substitutions substitutions;
      if (expected.has_value() && expected->declaration == declaration.name &&
          expected->arguments.size() == declaration.typeParameters.size())
        for (std::size_t index = 0; index < declaration.typeParameters.size(); ++index)
          substitutions.emplace(declaration.typeParameters[index], expected->arguments[index]);
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (std::size_t index = 0; index < call.arguments.size(); ++index) {
        std::optional<Type> argumentExpected;
        if (index < patterns.size()) {
          Type candidate = substitute(patterns[index], substitutions);
          if (!containsTypeParameter(candidate)) argumentExpected = candidate;
        }
        arguments.push_back(lowerExpression(*call.arguments[index], argumentExpected));
        if (index < patterns.size())
          inferTypeArguments(patterns[index], arguments.back()->type, substitutions,
                             arguments.back()->location);
      }
      if (arguments.size() != patterns.size())
        diagnostics_.error(expression.location, "constructor '" + name + "' expects " +
                                                 std::to_string(patterns.size()) + " argument(s)",
                           DiagnosticCode::Arity);
      std::vector<Type> typeArguments;
      for (const auto& parameter : declaration.typeParameters) {
        auto inferred = substitutions.find(parameter);
        if (inferred == substitutions.end()) {
          diagnostics_.error(expression.location, "cannot infer type argument '" + parameter +
                                                   "' for constructor '" + name +
                                                   "'; add a binding type annotation");
          typeArguments.push_back(Type::Invalid);
        } else {
          typeArguments.push_back(inferred->second);
        }
      }
      for (std::size_t index = 0; index < arguments.size() && index < patterns.size(); ++index) {
        const Type required = substitute(patterns[index], substitutions);
        if (arguments[index]->type != Type::Invalid && arguments[index]->type != required)
          diagnostics_.error(arguments[index]->location, "constructor argument type is " +
                                                          typeName(arguments[index]->type) +
                                                          ", expected " + typeName(required));
      }
      Type result{declaration.kind == HirTypeDeclKind::Struct ? TypeKind::Struct : TypeKind::Enum,
                  declaration.name, std::move(typeArguments)};
      if ((declaration.name == "std.collections.Map" ||
           declaration.name == "std.collections.Set") &&
          !result.arguments.empty() && !isHashableKey(result.arguments[0]))
        diagnostics_.error(expression.location,
                           "Map and Set keys must be Int, Bool, Char, or String");
      return std::make_unique<HirAggregateExpr>(expression.location, std::move(result),
                                                 *aggregateDeclaration, tag,
                                                 std::move(arguments));
    }

    auto standard = standardFunctions_.find(name);
    if (standard != standardFunctions_.end()) {
      const StandardFunction& definition = standard->second;
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (std::size_t index = 0; index < call.arguments.size(); ++index) {
        std::optional<Type> argumentExpected;
        if (index < definition.parameterTypes.size() &&
            !containsTypeParameter(definition.parameterTypes[index]))
          argumentExpected = definition.parameterTypes[index];
        arguments.push_back(lowerExpression(*call.arguments[index], argumentExpected));
      }
      if (arguments.size() != definition.parameterTypes.size())
        diagnostics_.error(expression.location, "standard function '" + name + "' expects " +
                                                 std::to_string(definition.parameterTypes.size()) +
                                                 " argument(s)", DiagnosticCode::Arity);
      Substitutions inferred;
      for (std::size_t index = 0;
           index < arguments.size() && index < definition.parameterTypes.size(); ++index) {
        if (!inferTypeArguments(definition.parameterTypes[index], arguments[index]->type,
                                inferred, arguments[index]->location))
          diagnostics_.error(arguments[index]->location, "argument type is " +
                                                          typeName(arguments[index]->type) +
                                                          ", expected " +
                                                          typeName(definition.parameterTypes[index]));
      }
      std::string key = name;
      if (!definition.typeParameters.empty()) key += '[';
      for (std::size_t index = 0; index < definition.typeParameters.size(); ++index) {
        const auto& parameter = definition.typeParameters[index];
        auto found = inferred.find(parameter);
        if (found == inferred.end()) {
          diagnostics_.error(expression.location, "cannot infer standard-library type argument '" +
                                                   parameter + "'");
          inferred.emplace(parameter, Type::Invalid);
          found = inferred.find(parameter);
        }
        if (index) key += ',';
        key += typeName(found->second);
      }
      if (!definition.typeParameters.empty()) key += ']';
      std::vector<Type> parameterTypes;
      for (const auto& parameter : definition.parameterTypes)
        parameterTypes.push_back(substitute(parameter, inferred));
      const Type result = substitute(definition.result, inferred);
      if (definition.intrinsic == Intrinsic::CollectionsMapFromArrays ||
          definition.intrinsic == Intrinsic::CollectionsMapLength ||
          definition.intrinsic == Intrinsic::CollectionsMapFind ||
          definition.intrinsic == Intrinsic::CollectionsMapGet ||
          definition.intrinsic == Intrinsic::CollectionsMapKeys ||
          definition.intrinsic == Intrinsic::CollectionsMapValues) {
        if (!isHashableKey(inferred.at("K")))
          diagnostics_.error(expression.location,
                             "Map keys must be Int, Bool, Char, or String");
      }
      if (definition.intrinsic == Intrinsic::CollectionsSetFromArray ||
          definition.intrinsic == Intrinsic::CollectionsSetContains ||
          definition.intrinsic == Intrinsic::CollectionsSetValues ||
          definition.intrinsic == Intrinsic::CollectionsHash ||
          definition.intrinsic == Intrinsic::CollectionsMapHash) {
        if (!isHashableKey(inferred.at("T")))
          diagnostics_.error(expression.location,
                             "Set elements and hash values must be Int, Bool, Char, or String");
      }
      if (definition.intrinsic == Intrinsic::CollectionsContains ||
          definition.intrinsic == Intrinsic::CollectionsFind ||
          definition.intrinsic == Intrinsic::CollectionsFilterEqual) {
        if (!isCollectionComparable(inferred.at("T")))
          diagnostics_.error(expression.location,
                             "collection equality requires Int, Float, Bool, Char, or String");
      }
      SymbolId callee = InvalidSymbol;
      if (auto found = specializations_.find(key); found != specializations_.end()) {
        callee = found->second;
      } else {
        callee = addSymbol(SymbolKind::BuiltinFunction, key, result, false,
                           {"<standard-library>", 1, 1}, parameterTypes,
                           definition.intrinsic);
        specializations_.emplace(key, callee);
      }
      for (std::size_t index = 0;
           index < arguments.size() && index < parameterTypes.size(); ++index)
        if (arguments[index]->type != Type::Invalid &&
            arguments[index]->type != parameterTypes[index])
          diagnostics_.error(arguments[index]->location, "argument type is " +
                                                          typeName(arguments[index]->type) +
                                                          ", expected " +
                                                          typeName(parameterTypes[index]));
      return std::make_unique<HirCallExpr>(expression.location, result, callee,
                                           std::move(arguments));
    }

    auto generic = genericFunctions_.find(name);
    if (generic != genericFunctions_.end()) {
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (const auto& argument : call.arguments) arguments.push_back(lowerExpression(*argument));
      const SymbolId callee = specializeFunction(*generic->second, arguments, expression.location);
      const Type result = callee == InvalidSymbol ? Type::Invalid : hir_.symbol(callee).type;
      return std::make_unique<HirCallExpr>(expression.location, result, callee,
                                           std::move(arguments));
    }

    auto found = functions_.find(name);
    if (found == functions_.end()) {
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (const auto& argument : call.arguments) arguments.push_back(lowerExpression(*argument));
      diagnostics_.error(call.callee->location,
                         "unknown function or constructor '" + name + "'",
                         DiagnosticCode::Name);
      return std::make_unique<HirCallExpr>(expression.location, Type::Invalid, InvalidSymbol,
                                           std::move(arguments));
    }
    const SymbolId callee = found->second;
    const HirSymbol signature = hir_.symbol(callee);
    std::vector<std::unique_ptr<HirExpr>> arguments;
    for (std::size_t index = 0; index < call.arguments.size(); ++index) {
      std::optional<Type> argumentExpected;
      if (signature.kind != SymbolKind::BuiltinFunction &&
          index < signature.parameterTypes.size())
        argumentExpected = signature.parameterTypes[index];
      arguments.push_back(lowerExpression(*call.arguments[index], argumentExpected));
    }
    if (signature.kind == SymbolKind::BuiltinFunction) {
      if (arguments.size() != 1)
        diagnostics_.error(expression.location, "print expects exactly one argument",
                           DiagnosticCode::Arity);
      else if (isCollectionType(arguments[0]->type) || isAggregateType(arguments[0]->type))
        diagnostics_.error(arguments[0]->location, "print does not accept aggregate values");
      return std::make_unique<HirCallExpr>(expression.location, Type::Unit, callee,
                                           std::move(arguments));
    }
    if (arguments.size() != signature.parameterTypes.size())
      diagnostics_.error(expression.location, "function '" + name + "' expects " +
                                                 std::to_string(signature.parameterTypes.size()) +
                                                 " argument(s)", DiagnosticCode::Arity);
    for (std::size_t index = 0;
         index < arguments.size() && index < signature.parameterTypes.size(); ++index)
      if (arguments[index]->type != Type::Invalid &&
          arguments[index]->type != signature.parameterTypes[index])
        diagnostics_.error(arguments[index]->location, "argument type is " +
                                                        typeName(arguments[index]->type) +
                                                        ", expected " +
                                                        typeName(signature.parameterTypes[index]));
    return std::make_unique<HirCallExpr>(expression.location, signature.type, callee,
                                         std::move(arguments));
  }
  case ExprKind::Array: {
    const auto& array = static_cast<const ArrayExpr&>(expression);
    std::optional<Type> expectedElement;
    if (expected.has_value() && isArrayType(*expected))
      expectedElement = collectionElementType(*expected);
    std::vector<std::unique_ptr<HirExpr>> elements;
    for (const auto& element : array.elements)
      elements.push_back(lowerExpression(*element, expectedElement));
    if (elements.empty()) {
      if (!expectedElement.has_value()) {
        diagnostics_.error(expression.location,
                           "empty Array literal requires an Array[T] context");
        return std::make_unique<HirArrayExpr>(expression.location, Type::Invalid,
                                              std::move(elements));
      }
      return std::make_unique<HirArrayExpr>(expression.location, arrayType(*expectedElement),
                                            std::move(elements));
    }
    const Type elementType = expectedElement.value_or(elements.front()->type);
    const Type result = arrayType(elementType);
    for (const auto& element : elements)
      if (element->type != Type::Invalid && element->type != elementType)
        diagnostics_.error(element->location, "Array literal elements must have one type; found " +
                                                 typeName(element->type) + ", expected " +
                                                 typeName(elementType));
    return std::make_unique<HirArrayExpr>(expression.location, result, std::move(elements));
  }
  case ExprKind::Index: {
    const auto& index = static_cast<const IndexExpr&>(expression);
    auto collection = lowerExpression(*index.collection);
    auto offset = lowerExpression(*index.index, Type::Int);
    if (collection->type != Type::Invalid && !isCollectionType(collection->type))
      diagnostics_.error(index.collection->location, "indexing requires an Array or Slice value");
    if (offset->type != Type::Invalid && offset->type != Type::Int)
      diagnostics_.error(index.index->location, "collection index must have type Int");
    return std::make_unique<HirIndexExpr>(expression.location,
                                          collectionElementType(collection->type),
                                          std::move(collection), std::move(offset));
  }
  case ExprKind::Slice: {
    const auto& slice = static_cast<const SliceExpr&>(expression);
    auto collection = lowerExpression(*slice.collection);
    auto start = lowerExpression(*slice.start, Type::Int);
    auto end = lowerExpression(*slice.end, Type::Int);
    if (collection->type != Type::Invalid && !isCollectionType(collection->type))
      diagnostics_.error(slice.collection->location, "slicing requires an Array or Slice value");
    if (start->type != Type::Invalid && start->type != Type::Int)
      diagnostics_.error(slice.start->location, "slice start must have type Int");
    if (end->type != Type::Invalid && end->type != Type::Int)
      diagnostics_.error(slice.end->location, "slice end must have type Int");
    return std::make_unique<HirSliceExpr>(expression.location,
                                          sliceType(collectionElementType(collection->type)),
                                          std::move(collection), std::move(start), std::move(end));
  }
  case ExprKind::Field: {
    const auto& field = static_cast<const FieldExpr&>(expression);
    if (field.value->kind == ExprKind::Name) {
      const auto& owner = static_cast<const LiteralExpr&>(*field.value);
      const std::string target = owner.value + "." + field.field;
      if (associatedConstants_.contains(target)) {
        std::vector<std::unique_ptr<HirExpr>> arguments;
        return lowerResolvedCall(target, expression.location, std::move(arguments));
      }
    }
    auto value = lowerExpression(*field.value);
    const std::uint32_t declarationIndex = findTypeDeclaration(value->type);
    if (value->type.kind != TypeKind::Struct ||
        declarationIndex == static_cast<std::uint32_t>(-1)) {
      diagnostics_.error(expression.location, "field access requires a struct value");
      return std::make_unique<HirFieldExpr>(expression.location, Type::Invalid,
                                            std::move(value), 0);
    }
    const auto& declaration = hir_.typeDeclarations[declarationIndex];
    std::optional<std::uint32_t> index;
    for (std::uint32_t candidate = 0; candidate < declaration.fields.size(); ++candidate)
      if (declaration.fields[candidate].name == field.field) index = candidate;
    if (!index.has_value()) {
      diagnostics_.error(expression.location, "struct '" + declaration.name +
                                                  "' has no field '" + field.field + "'");
      return std::make_unique<HirFieldExpr>(expression.location, Type::Invalid,
                                            std::move(value), 0);
    }
    Substitutions substitutions;
    for (std::size_t argument = 0; argument < declaration.typeParameters.size(); ++argument)
      substitutions.emplace(declaration.typeParameters[argument], value->type.arguments[argument]);
    const Type result = substitute(declaration.fields[*index].type, substitutions);
    return std::make_unique<HirFieldExpr>(expression.location, result, std::move(value), *index);
  }
  case ExprKind::Propagate: {
    const auto& propagate = static_cast<const PropagateExpr&>(expression);
    auto value = lowerExpression(*propagate.value);
    const std::uint32_t declarationIndex = findTypeDeclaration(value->type);
    if (value->type.kind != TypeKind::Enum ||
        declarationIndex == static_cast<std::uint32_t>(-1) ||
        (value->type.declaration != "Option" && value->type.declaration != "Result")) {
      diagnostics_.error(expression.location, "'?' requires Option[T] or Result[T, E]");
      return std::make_unique<HirPropagateExpr>(expression.location, Type::Invalid,
                                                std::move(value), currentReturnType_,
                                                declarationIndex);
    }
    if (currentReturnType_.kind != TypeKind::Enum ||
        currentReturnType_.declaration != value->type.declaration) {
      diagnostics_.error(expression.location, "'?' in this function requires return type " +
                                                  value->type.declaration + "[...]");
    } else if (value->type.declaration == "Result" &&
               value->type.arguments[1] != currentReturnType_.arguments[1]) {
      diagnostics_.error(expression.location, "'?' error type is " +
                                                  typeName(value->type.arguments[1]) +
                                                  ", function returns " +
                                                  typeName(currentReturnType_.arguments[1]));
    }
    const Type success = value->type.arguments.front();
    return std::make_unique<HirPropagateExpr>(expression.location, success,
                                              std::move(value), currentReturnType_,
                                              declarationIndex);
  }
  case ExprKind::Lambda: {
    const auto& lambda = static_cast<const LambdaExpr&>(expression);
    std::unordered_set<std::string> parameterNames;
    std::vector<Type> parameterTypes;
    for (const auto& parameter : lambda.parameters) {
      if (!parameterNames.insert(parameter.name).second)
        diagnostics_.error(parameter.location,
                           "duplicate lambda parameter '" + parameter.name + "'");
      parameterTypes.push_back(resolveType(parameter.typeName, parameter.location,
                                           currentSubstitutions_));
    }
    const Type resultType = resolveType(lambda.returnType, lambda.location,
                                        currentSubstitutions_);
    std::vector<LambdaCapture> captures;
    collectLambdaCaptures(*lambda.body, parameterNames, captures);
    const std::string closureName = "$closure." +
                                    std::to_string(pendingLambdas_.size());
    HirTypeDeclaration declaration;
    declaration.kind = HirTypeDeclKind::Struct;
    declaration.name = closureName;
    declaration.location = lambda.location;
    declaration.builtin = true;
    for (const auto& capture : captures)
      declaration.fields.push_back({capture.name, hir_.symbol(capture.source).type,
                                    lambda.location});
    const std::uint32_t declarationIndex =
        static_cast<std::uint32_t>(hir_.typeDeclarations.size());
    typeDeclarations_.emplace(closureName, declarationIndex);
    hir_.typeDeclarations.push_back(std::move(declaration));
    const Type closureType{TypeKind::Struct, closureName};
    std::vector<Type> callableParameters{closureType};
    callableParameters.insert(callableParameters.end(), parameterTypes.begin(),
                              parameterTypes.end());
    const std::string callableName = closureName + ".call";
    const SymbolId callable = addSymbol(SymbolKind::Function, callableName,
                                        resultType, false, lambda.location,
                                        callableParameters);
    functions_.emplace(callableName, callable);
    pendingLambdas_.push_back(
        {&lambda, callable, declarationIndex, closureType, captures});
    std::vector<std::unique_ptr<HirExpr>> capturedValues;
    for (const auto& capture : captures)
      capturedValues.push_back(std::make_unique<HirNameExpr>(
          lambda.location, hir_.symbol(capture.source).type, capture.source));
    return std::make_unique<HirAggregateExpr>(
        expression.location, closureType, declarationIndex, 0,
        std::move(capturedValues));
  }
  }
  return std::make_unique<HirLiteralExpr>(expression.location, Type::Invalid, "0");
}

SymbolId HirLowerer::findVariable(const std::string& name) const {
  for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
    auto found = scope->find(name);
    if (found != scope->end()) return found->second;
  }
  return InvalidSymbol;
}

bool HirLowerer::definitelyReturns(const HirBlock& body) const {
  for (const auto& statement : body) {
    if (statement->kind == HirStmtKind::Return) return true;
    if (statement->kind == HirStmtKind::If) {
      const auto& branch = static_cast<const HirIfStmt&>(*statement);
      if (!branch.elseBody.empty() && definitelyReturns(branch.thenBody) &&
          definitelyReturns(branch.elseBody))
        return true;
    }
    if (statement->kind == HirStmtKind::Match) {
      const auto& match = static_cast<const HirMatchStmt&>(*statement);
      if (!match.cases.empty()) {
        bool allReturn = true;
        for (const auto& matchCase : match.cases)
          allReturn = definitelyReturns(matchCase.body) && allReturn;
        if (allReturn) return true;
      }
    }
  }
  return false;
}

} // namespace rocket
