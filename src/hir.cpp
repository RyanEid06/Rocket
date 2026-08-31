#include "hir.h"

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
#include <utility>

namespace rocket {

const std::vector<std::pair<std::string, std::vector<std::string>>>&
compilerCallableParameterNames() {
  static const std::vector<std::pair<std::string, std::vector<std::string>>> names = {
      {"print", {"value"}},
      {"std.math.pi", {}}, {"std.math.tau", {}}, {"std.math.e", {}},
      {"std.math.abs", {"value"}}, {"std.math.abs_int", {"value"}},
      {"std.math.min", {"left", "right"}}, {"std.math.max", {"left", "right"}},
      {"std.math.min_int", {"left", "right"}}, {"std.math.max_int", {"left", "right"}},
      {"std.math.clamp", {"value", "minimum", "maximum"}},
      {"std.math.clamp_int", {"value", "minimum", "maximum"}},
      {"std.math.sign", {"value"}}, {"std.math.sign_int", {"value"}},
      {"std.math.floor", {"value"}}, {"std.math.ceil", {"value"}},
      {"std.math.round", {"value"}}, {"std.math.trunc", {"value"}},
      {"std.math.fract", {"value"}}, {"std.math.sqrt", {"value"}},
      {"std.math.pow", {"base", "exponent"}}, {"std.math.exp", {"value"}},
      {"std.math.log", {"value"}}, {"std.math.log10", {"value"}},
      {"std.math.sin", {"radians"}}, {"std.math.cos", {"radians"}},
      {"std.math.tan", {"radians"}}, {"std.math.asin", {"value"}},
      {"std.math.acos", {"value"}}, {"std.math.atan", {"value"}},
      {"std.math.atan2", {"y", "x"}}, {"std.math.radians", {"degrees"}},
      {"std.math.degrees", {"radians"}}, {"std.math.lerp", {"start", "end", "progress"}},
      {"std.math.inverse_lerp", {"start", "end", "value"}},
      {"std.math.remap", {"input_start", "input_end", "output_start", "output_end", "value"}},
      {"std.math.smoothstep", {"start", "end", "value"}},
      {"std.math.smootherstep", {"start", "end", "value"}},
      {"std.math.approach", {"current", "target", "maximum_delta"}},
      {"std.math.move_towards", {"current", "target", "maximum_delta"}},
      {"std.string.byte_length", {"value"}},
      {"std.string.concat", {"left", "right"}},
      {"std.string.contains", {"value", "needle"}},
      {"std.string.starts_with", {"value", "prefix"}},
      {"std.string.ends_with", {"value", "suffix"}},
      {"std.string.trim", {"value"}},
      {"std.string.split", {"value", "delimiter"}},
      {"std.string.byte_at", {"value", "index"}},
      {"std.string.byte_value_at", {"value", "index"}},
      {"std.string.slice", {"value", "start", "end"}},
      {"std.string.parse_int", {"value"}},
      {"std.string.from_int", {"value"}},
      {"std.string.builder", {}},
      {"std.string.builder_append", {"builder", "value"}},
      {"std.string.builder_finish", {"builder"}},
      {"std.collections.length", {"values"}},
      {"std.collections.slice_length", {"values"}},
      {"std.collections.capacity", {"values"}},
      {"std.collections.reserve", {"values", "minimum"}},
      {"std.collections.append", {"values", "value"}},
      {"std.collections.pop", {"values"}},
      {"std.collections.insert", {"values", "index", "value"}},
      {"std.collections.remove", {"values", "index"}},
      {"std.collections.clear", {"values"}},
      {"std.collections.map_from_arrays", {"keys", "values"}},
      {"std.collections.map_length", {"map"}},
      {"std.collections.map_find", {"map", "key"}},
      {"std.collections.map_get", {"map", "key"}},
      {"std.collections.map_keys", {"map"}},
      {"std.collections.map_values", {"map"}},
      {"std.collections.set_from_array", {"values"}},
      {"std.collections.set_contains", {"set", "value"}},
      {"std.collections.set_values", {"set"}},
      {"std.collections.hash", {"value"}},
      {"std.collections.contains", {"values", "value"}},
      {"std.collections.find", {"values", "value"}},
      {"std.collections.filter_equal", {"values", "value"}},
      {"std.collections.sort_int", {"values"}},
      {"std.collections.sort_float", {"values"}},
      {"std.collections.sort_char", {"values"}},
      {"std.collections.sort_string", {"values"}},
      {"std.collections.map_hash", {"values"}},
      {"std.collections.fold_sum_int", {"values"}},
      {"std.collections.fold_sum_float", {"values"}},
      {"std.collections.reverse", {"values"}},
      {"std.collections.concat", {"left", "right"}},
      {"std.collections.join", {"values", "separator"}},
      {"std.file.read_text", {"path"}},
      {"std.file.write_text", {"path", "contents"}},
      {"std.file.append_text", {"path", "contents"}},
      {"std.file.exists", {"path"}},
      {"std.file.remove", {"path"}},
      {"std.file.list", {"path"}},
      {"std.file.create_directory", {"path"}},
      {"std.file.read_binary", {"path"}},
      {"std.file.write_binary", {"path", "buffer"}},
      {"std.file.append_binary", {"path", "buffer"}},
      {"std.binary.from_string", {"value"}},
      {"std.binary.to_string", {"buffer"}},
      {"std.binary.length", {"buffer"}},
      {"std.binary.slice", {"buffer", "offset", "length"}},
      {"std.binary.read_u8", {"buffer", "offset"}},
      {"std.binary.read_u16_le", {"buffer", "offset"}},
      {"std.binary.read_u32_le", {"buffer", "offset"}},
      {"std.binary.write_u8", {"value"}},
      {"std.binary.write_u16_le", {"value"}},
      {"std.binary.write_u32_le", {"value"}},
      {"std.binary.concat", {"left", "right"}},
      {"std.binary.read_u16_be", {"buffer", "offset"}},
      {"std.binary.read_u32_be", {"buffer", "offset"}},
      {"std.binary.write_u16_be", {"value"}},
      {"std.binary.write_u32_be", {"value"}},
      {"std.stream.open_reader", {"path", "buffer_size"}},
      {"std.stream.read", {"handle", "maximum"}},
      {"std.stream.close_reader", {"handle"}},
      {"std.stream.open_writer", {"path", "buffer_size", "append"}},
      {"std.stream.write", {"handle", "buffer"}},
      {"std.stream.flush", {"handle"}},
      {"std.stream.close_writer", {"handle"}},
      {"std.unicode.scalar_count", {"value"}},
      {"std.unicode.scalar_at", {"value", "index"}},
      {"std.unicode.from_scalar", {"value"}},
      {"std.unicode.normalize_nfc", {"value"}},
      {"std.unicode.normalize_nfd", {"value"}},
      {"std.unicode.grapheme_count", {"value"}},
      {"std.unicode.grapheme_at", {"value", "index"}},
      {"std.regex.is_match", {"pattern", "value"}},
      {"std.regex.find_all", {"pattern", "value"}},
      {"std.regex.replace_all", {"pattern", "value", "replacement"}},
      {"std.crypto.secure_bytes", {"length"}},
      {"std.crypto.secure_int", {"minimum", "maximum"}},
      {"std.crypto.sha256", {"value"}},
      {"std.crypto.hmac_sha256", {"key", "value"}},
      {"std.crypto.constant_time_equal", {"left", "right"}},
      {"std.crypto.verify_signed_file", {"path"}},
      {"std.net.resolve", {"host", "service"}},
      {"std.net.tcp_connect", {"host", "port", "timeout_ms"}},
      {"std.net.tcp_listen", {"address", "port", "backlog"}},
      {"std.net.accept", {"listener", "timeout_ms"}},
      {"std.net.send", {"handle", "bytes", "timeout_ms"}},
      {"std.net.receive", {"handle", "maximum", "timeout_ms"}},
      {"std.net.close", {"handle"}},
      {"std.net.cancel", {"handle"}},
      {"std.net.local_port", {"handle"}},
      {"std.http.request", {"method", "url", "body", "timeout_ms"}},
      {"std.http.read_request", {"connection", "maximum", "timeout_ms"}},
      {"std.http.write_response",
       {"connection", "status", "content_type", "body", "timeout_ms"}},
      {"std.datetime.format_utc", {"unix_ms"}},
      {"std.datetime.parse_utc", {"text"}},
      {"std.datetime.days_in_month", {"year", "month"}},
      {"std.datetime.weekday", {"year", "month", "day"}},
      {"std.datetime.local_offset_minutes", {"unix_ms"}},
      {"std.datetime.timezone_name", {}},
      {"std.log.write", {"level", "message"}},
      {"std.log.append", {"path", "level", "message"}},
      {"std.cli.has_flag", {"arguments", "name"}},
      {"std.cli.option", {"arguments", "name"}},
      {"std.cli.positionals", {"arguments"}},
      {"std.config.get", {"text", "key"}},
      {"std.config.load", {"path", "key"}},
      {"std.compression.xpress_compress", {"bytes"}},
      {"std.compression.xpress_decompress", {"bytes"}},
      {"std.archive.tar_create", {"path", "names", "contents"}},
      {"std.archive.tar_list", {"path"}},
      {"std.archive.tar_read", {"path", "name"}},
      {"std.sqlite.open", {"path"}},
      {"std.sqlite.execute", {"handle", "sql", "parameters"}},
      {"std.sqlite.query", {"handle", "sql", "parameters"}},
      {"std.sqlite.close", {"handle"}},
      {"std.testing_core.assert", {"condition", "message"}},
      {"std.testing_core.equal_int", {"actual", "expected", "message"}},
      {"std.testing_core.equal_string", {"actual", "expected", "message"}},
      {"std.testing_core.temp_directory", {"prefix"}},
      {"std.testing_core.fixture_path", {"root", "relative"}},
      {"std.testing_core.cleanup_temp", {"root"}},
      {"std.testing_core.coverage_hit", {"name"}},
      {"std.testing_core.coverage_write", {"path"}},
      {"std.path.join", {"left", "right"}},
      {"std.path.basename", {"path"}},
      {"std.path.extension", {"path"}},
      {"std.path.normalize", {"path"}},
      {"std.json.parse", {"text"}},
      {"std.json.stringify", {"value"}},
      {"std.csv.parse", {"text"}},
      {"std.csv.encode", {"rows"}},
      {"std.random.seed", {"value"}},
      {"std.random.int", {"minimum", "maximum"}},
      {"std.random.float", {}},
      {"std.process.run", {"program", "arguments"}},
      {"std.process.arguments", {}},
      {"std.process.executable_path", {}},
      {"std.process.environment", {"name"}},
      {"std.process.working_directory", {}},
      {"std.target.alias", {}},
      {"std.target.triple", {}},
      {"std.target.os", {}},
      {"std.target.architecture", {}},
      {"std.target.environment", {}},
      {"std.target.pointer_width", {}},
      {"std.target.endianness", {}},
      {"std.target.has_feature", {"name"}},
      {"std.time.unix_milliseconds", {}},
      {"std.time.monotonic_milliseconds", {}},
      {"std.time.sleep_milliseconds", {"value"}},
      {"std.task.join", {"task"}},
      {"std.task.is_complete", {"task"}},
      {"std.task.cancel", {"task"}},
      {"std.task.group", {"tasks"}},
      {"std.task.group_join", {"group"}},
      {"std.task.group_cancel", {"group"}},
      {"std.thread.spawn", {"task"}},
      {"std.thread.join", {"thread"}},
      {"std.thread.detach", {"thread"}},
      {"std.thread.is_complete", {"thread"}},
      {"std.ownership.downgrade", {"value"}},
      {"std.ownership.upgrade", {"value"}},
      {"std.ownership.expired", {"value"}},
      {"std.buffer.thaw", {"values"}},
      {"std.buffer.length", {"buffer"}},
      {"std.buffer.capacity", {"buffer"}},
      {"std.buffer.get", {"buffer", "index"}},
      {"std.buffer.set", {"buffer", "index", "value"}},
      {"std.buffer.append", {"buffer", "value"}},
      {"std.buffer.slice", {"buffer", "start", "end"}},
      {"std.buffer.freeze", {"buffer"}},
      {"std.cancel.token", {}},
      {"std.cancel.child", {"parent"}},
      {"std.cancel.current", {}},
      {"std.cancel.cancel", {"token"}},
      {"std.cancel.is_cancelled", {"token"}},
      {"std.cancel.check", {"token"}},
      {"std.async_time.deadline_after", {"milliseconds"}},
      {"std.async_time.remaining", {"deadline"}},
      {"std.async_time.sleep", {"milliseconds", "token"}},
      {"std.async_time.sleep_until", {"deadline", "token"}},
      {"std.async_file.read", {"path", "maximum", "token"}},
      {"std.async_file.write", {"path", "bytes", "append", "token"}},
      {"std.async_net.connect", {"host", "port", "deadline", "token"}},
      {"std.async_net.accept", {"listener", "deadline", "token"}},
      {"std.async_net.receive", {"socket", "maximum", "deadline", "token"}},
      {"std.async_net.send", {"socket", "bytes", "deadline", "token"}},
      {"std.async_net.shutdown", {"socket"}},
      {"std.async_process.run", {"program", "arguments", "deadline", "token"}},
      {"std.sync.mutex", {"value"}},
      {"std.sync.lock", {"mutex", "deadline", "token"}},
      {"std.sync.guard_get", {"guard"}},
      {"std.sync.guard_set", {"guard", "value"}},
      {"std.sync.unlock", {"guard"}},
      {"std.sync.event", {"manual_reset", "initially_set"}},
      {"std.sync.event_set", {"event"}},
      {"std.sync.event_reset", {"event"}},
      {"std.sync.event_wait", {"event", "deadline", "token"}},
      {"std.sync.atomic_int", {"value"}},
      {"std.sync.atomic_load", {"value"}},
      {"std.sync.atomic_store", {"value", "replacement"}},
      {"std.sync.atomic_fetch_add", {"value", "delta"}},
      {"std.sync.atomic_compare_exchange", {"value", "expected", "replacement"}},
      {"std.sync.once", {"value"}},
      {"std.sync.once_empty", {"type_witness"}},
      {"std.sync.once_set", {"cell", "value"}},
      {"std.sync.once_get", {"cell"}},
      {"std.channel.bounded", {"initial", "capacity"}},
      {"std.channel.unbounded", {"initial"}},
      {"std.channel.sender", {"channel"}},
      {"std.channel.receiver", {"channel"}},
      {"std.channel.clone_sender", {"sender"}},
      {"std.channel.clone_receiver", {"receiver"}},
      {"std.channel.send", {"sender", "value", "deadline", "token"}},
      {"std.channel.receive", {"receiver", "deadline", "token"}},
      {"std.channel.close_sender", {"sender"}},
      {"std.channel.close_receiver", {"receiver"}},
  };
  return names;
}

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

std::optional<Type> asyncSuccessType(const Type& result) {
  if (result.kind != TypeKind::Enum || result.declaration != "Result" ||
      result.arguments.size() != 2 || result.arguments[1] != Type::String)
    return std::nullopt;
  return result.arguments[0];
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

const Expr& callArgumentValue(const Expr& argument) {
  if (argument.kind == ExprKind::NamedArgument)
    return *static_cast<const NamedArgumentExpr&>(argument).value;
  return argument;
}

bool hasNamedArguments(const std::vector<std::unique_ptr<Expr>>& arguments) {
  return std::any_of(arguments.begin(), arguments.end(), [](const auto& argument) {
    return argument->kind == ExprKind::NamedArgument;
  });
}

std::size_t editDistance(const std::string& left, const std::string& right) {
  std::vector<std::size_t> previous(right.size() + 1);
  std::vector<std::size_t> current(right.size() + 1);
  for (std::size_t index = 0; index <= right.size(); ++index) previous[index] = index;
  for (std::size_t leftIndex = 0; leftIndex < left.size(); ++leftIndex) {
    current[0] = leftIndex + 1;
    for (std::size_t rightIndex = 0; rightIndex < right.size(); ++rightIndex) {
      const std::size_t replacement = previous[rightIndex] +
          (left[leftIndex] == right[rightIndex] ? 0u : 1u);
      current[rightIndex + 1] = std::min(
          {previous[rightIndex + 1] + 1, current[rightIndex] + 1, replacement});
    }
    previous.swap(current);
  }
  return previous.back();
}

struct NamedArgumentBinding {
  std::vector<std::optional<std::size_t>> sourceToParameter;
  std::vector<std::optional<std::size_t>> parameterToSource;
  std::vector<std::size_t> evaluationOrder;
};

NamedArgumentBinding bindNamedArguments(
    const std::vector<std::unique_ptr<Expr>>& arguments,
    const std::vector<std::string>& parameterNames, const Location& callLocation,
    Diagnostics& diagnostics, const std::vector<bool>& defaultable = {}) {
  NamedArgumentBinding result;
  result.sourceToParameter.resize(arguments.size());
  result.parameterToSource.resize(parameterNames.size());
  std::size_t nextPositional = 0;
  for (std::size_t source = 0; source < arguments.size(); ++source) {
    const Expr& argument = *arguments[source];
    std::optional<std::size_t> parameter;
    if (argument.kind == ExprKind::NamedArgument) {
      const auto& named = static_cast<const NamedArgumentExpr&>(argument);
      auto found = std::find(parameterNames.begin(), parameterNames.end(), named.name);
      if (found == parameterNames.end()) {
        std::string message = "unknown named argument '" + named.name + "'";
        std::size_t bestDistance = static_cast<std::size_t>(-1);
        std::string best;
        for (const auto& candidate : parameterNames) {
          const std::size_t distance = editDistance(named.name, candidate);
          if (distance < bestDistance ||
              (distance == bestDistance && (best.empty() || candidate < best))) {
            bestDistance = distance;
            best = candidate;
          }
        }
        if (!best.empty() && bestDistance <= 2)
          message += "; did you mean '" + best + "'?";
        diagnostics.error(named.location, std::move(message), DiagnosticCode::Name);
        continue;
      }
      parameter = static_cast<std::size_t>(found - parameterNames.begin());
    } else {
      if (nextPositional >= parameterNames.size()) {
        diagnostics.error(argument.location, "too many positional arguments",
                          DiagnosticCode::Arity);
        continue;
      }
      parameter = nextPositional++;
    }
    if (result.parameterToSource[*parameter].has_value()) {
      const std::size_t previousSource = *result.parameterToSource[*parameter];
      const bool previousNamed =
          arguments[previousSource]->kind == ExprKind::NamedArgument;
      const std::string& name = parameterNames[*parameter];
      diagnostics.error(
          argument.location,
          previousNamed ? "duplicate named argument '" + name + "'"
                        : "argument '" + name +
                              "' is already supplied positionally",
          DiagnosticCode::Arity);
      continue;
    }
    result.sourceToParameter[source] = parameter;
    result.parameterToSource[*parameter] = source;
    result.evaluationOrder.push_back(*parameter);
  }
  for (std::size_t parameter = 0; parameter < parameterNames.size(); ++parameter)
    if (!result.parameterToSource[parameter].has_value() &&
        (parameter >= defaultable.size() || !defaultable[parameter]))
      diagnostics.error(callLocation,
                        "missing required argument '" + parameterNames[parameter] + "'",
                        DiagnosticCode::Arity);
  return result;
}

} // namespace

SymbolId HirLowerer::addSymbol(SymbolKind kind, const std::string& name, Type type,
                               bool mutableBinding, const Location& location,
                               std::vector<Type> parameterTypes, Intrinsic intrinsic) {
  const SymbolId id = static_cast<SymbolId>(hir_.symbols.size());
  hir_.symbols.push_back({id, kind, name, std::move(type), mutableBinding, location,
                          std::move(parameterTypes), {}, {}, intrinsic});
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
  option.variants = {{"Some", option.location, {}, {typeParameter("T")}},
                     {"None", option.location, {}, {}}};
  typeDeclarations_.emplace(option.name, static_cast<std::uint32_t>(hir_.typeDeclarations.size()));
  hir_.typeDeclarations.push_back(std::move(option));

  HirTypeDeclaration result;
  result.kind = HirTypeDeclKind::Enum;
  result.name = "Result";
  result.location = {"<builtin>", 1, 1};
  result.publicDeclaration = true;
  result.builtin = true;
  result.typeParameters = {"T", "E"};
  result.variants = {{"Ok", result.location, {}, {typeParameter("T")}},
                     {"Err", result.location, {}, {typeParameter("E")}}};
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

  const Type byteBufferType{TypeKind::Struct, "std.collections.ByteBuffer"};
  addCollectionStruct(
      "std.http.Response", {},
      {{"status", Type::Int, collectionLocation},
       {"body", byteBufferType, collectionLocation}});
  addCollectionStruct(
      "std.http.Request", {},
      {{"method", Type::String, collectionLocation},
       {"path", Type::String, collectionLocation},
       {"body", byteBufferType, collectionLocation}});

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
      {"std.json.Null", json.location, {}, {}},
      {"std.json.Boolean", json.location, {}, {Type::Bool}},
      {"std.json.Integer", json.location, {}, {Type::Int}},
      {"std.json.Decimal", json.location, {}, {Type::Float}},
      {"std.json.Text", json.location, {}, {Type::String}},
      {"std.json.List", json.location, {}, {arrayType(jsonType)}},
      {"std.json.Object", json.location, {}, {arrayType(jsonFieldType)}},
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
  const Type byteBuffer{TypeKind::Struct, "std.collections.ByteBuffer"};
  const Type httpResponse{TypeKind::Struct, "std.http.Response"};
  const Type httpRequest{TypeKind::Struct, "std.http.Request"};
  const Type nestedStrings = arrayType(arrayType(Type::String));
  auto result = [](Type success) {
    return Type{TypeKind::Enum, "Result", {std::move(success), Type::String}};
  };
  auto add = [&](std::string name, std::vector<Type> parameters, Type returned,
                  Intrinsic intrinsic, std::vector<std::string> typeParameters = {}) {
    const auto& callableNames = compilerCallableParameterNames();
    const auto found = std::find_if(
        callableNames.begin(), callableNames.end(),
        [&](const auto& entry) { return entry.first == name; });
    if (found == callableNames.end() || found->second.size() != parameters.size())
      throw std::logic_error("invalid compiler callable metadata for " + name);
    standardFunctions_.emplace(
        std::move(name),
        StandardFunction{std::move(typeParameters), found->second,
                         std::move(parameters),
                         std::move(returned), intrinsic});
  };

  add("std.math.pi", {}, Type::Float, Intrinsic::Math);
  add("std.math.tau", {}, Type::Float, Intrinsic::Math);
  add("std.math.e", {}, Type::Float, Intrinsic::Math);
  add("std.math.abs", {Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.abs_int", {Type::Int}, Type::Int, Intrinsic::Math);
  add("std.math.min", {Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.max", {Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.min_int", {Type::Int, Type::Int}, Type::Int, Intrinsic::Math);
  add("std.math.max_int", {Type::Int, Type::Int}, Type::Int, Intrinsic::Math);
  add("std.math.clamp", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.clamp_int", {Type::Int, Type::Int, Type::Int}, Type::Int, Intrinsic::Math);
  add("std.math.sign", {Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.sign_int", {Type::Int}, Type::Int, Intrinsic::Math);
  for (const char* name : {"floor", "ceil", "round", "trunc", "fract", "sqrt", "exp", "log", "log10", "sin", "cos", "tan", "asin", "acos", "atan", "radians", "degrees"})
    add(std::string("std.math.") + name, {Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.pow", {Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.atan2", {Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.lerp", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.inverse_lerp", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.remap", {Type::Float, Type::Float, Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.smoothstep", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.smootherstep", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.approach", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);
  add("std.math.move_towards", {Type::Float, Type::Float, Type::Float}, Type::Float, Intrinsic::Math);

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
  add("std.file.read_binary", {Type::String}, result(byteBuffer),
      Intrinsic::FileReadBinary);
  add("std.file.write_binary", {Type::String, byteBuffer}, result(Type::Bool),
      Intrinsic::FileWriteBinary);
  add("std.file.append_binary", {Type::String, byteBuffer}, result(Type::Bool),
      Intrinsic::FileAppendBinary);

  add("std.binary.from_string", {Type::String}, byteBuffer,
      Intrinsic::BinaryFromString);
  add("std.binary.to_string", {byteBuffer}, result(Type::String),
      Intrinsic::BinaryToString);
  add("std.binary.length", {byteBuffer}, Type::Int, Intrinsic::BinaryLength);
  add("std.binary.slice", {byteBuffer, Type::Int, Type::Int}, result(byteBuffer),
      Intrinsic::BinarySlice);
  add("std.binary.read_u8", {byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::BinaryReadU8);
  add("std.binary.read_u16_le", {byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::BinaryReadU16Le);
  add("std.binary.read_u32_le", {byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::BinaryReadU32Le);
  add("std.binary.write_u8", {Type::Int}, result(byteBuffer),
      Intrinsic::BinaryWriteU8);
  add("std.binary.write_u16_le", {Type::Int}, result(byteBuffer),
      Intrinsic::BinaryWriteU16Le);
  add("std.binary.write_u32_le", {Type::Int}, result(byteBuffer),
      Intrinsic::BinaryWriteU32Le);
  add("std.binary.concat", {byteBuffer, byteBuffer}, byteBuffer,
      Intrinsic::BinaryConcat);
  add("std.binary.read_u16_be", {byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::BinaryReadU16Be);
  add("std.binary.read_u32_be", {byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::BinaryReadU32Be);
  add("std.binary.write_u16_be", {Type::Int}, result(byteBuffer),
      Intrinsic::BinaryWriteU16Be);
  add("std.binary.write_u32_be", {Type::Int}, result(byteBuffer),
      Intrinsic::BinaryWriteU32Be);

  add("std.stream.open_reader", {Type::String, Type::Int}, result(Type::Int),
      Intrinsic::StreamOpenReader);
  add("std.stream.read", {Type::Int, Type::Int}, result(byteBuffer),
      Intrinsic::StreamRead);
  add("std.stream.close_reader", {Type::Int}, result(Type::Bool),
      Intrinsic::StreamCloseReader);
  add("std.stream.open_writer", {Type::String, Type::Int, Type::Bool},
      result(Type::Int), Intrinsic::StreamOpenWriter);
  add("std.stream.write", {Type::Int, byteBuffer}, result(Type::Bool),
      Intrinsic::StreamWrite);
  add("std.stream.flush", {Type::Int}, result(Type::Bool),
      Intrinsic::StreamFlush);
  add("std.stream.close_writer", {Type::Int}, result(Type::Bool),
      Intrinsic::StreamCloseWriter);

  add("std.unicode.scalar_count", {Type::String}, Type::Int,
      Intrinsic::UnicodeScalarCount);
  add("std.unicode.scalar_at", {Type::String, Type::Int}, result(Type::Int),
      Intrinsic::UnicodeScalarAt);
  add("std.unicode.from_scalar", {Type::Int}, result(Type::String),
      Intrinsic::UnicodeFromScalar);
  add("std.unicode.normalize_nfc", {Type::String}, result(Type::String),
      Intrinsic::UnicodeNormalizeNfc);
  add("std.unicode.normalize_nfd", {Type::String}, result(Type::String),
      Intrinsic::UnicodeNormalizeNfd);
  add("std.unicode.grapheme_count", {Type::String}, Type::Int,
      Intrinsic::UnicodeGraphemeCount);
  add("std.unicode.grapheme_at", {Type::String, Type::Int}, result(Type::String),
      Intrinsic::UnicodeGraphemeAt);

  add("std.regex.is_match", {Type::String, Type::String}, result(Type::Bool),
      Intrinsic::RegexIsMatch);
  add("std.regex.find_all", {Type::String, Type::String},
      result(arrayType(Type::String)), Intrinsic::RegexFindAll);
  add("std.regex.replace_all", {Type::String, Type::String, Type::String},
      result(Type::String), Intrinsic::RegexReplaceAll);

  add("std.crypto.secure_bytes", {Type::Int}, result(byteBuffer),
      Intrinsic::CryptoSecureBytes);
  add("std.crypto.secure_int", {Type::Int, Type::Int}, result(Type::Int),
      Intrinsic::CryptoSecureInt);
  add("std.crypto.sha256", {byteBuffer}, result(Type::String),
      Intrinsic::CryptoSha256);
  add("std.crypto.hmac_sha256", {byteBuffer, byteBuffer}, result(Type::String),
      Intrinsic::CryptoHmacSha256);
  add("std.crypto.constant_time_equal", {byteBuffer, byteBuffer}, Type::Bool,
      Intrinsic::CryptoConstantTimeEqual);
  add("std.crypto.verify_signed_file", {Type::String}, result(Type::Bool),
      Intrinsic::CryptoVerifySignedFile);

  add("std.net.resolve", {Type::String, Type::String},
      result(arrayType(Type::String)), Intrinsic::NetResolve);
  add("std.net.tcp_connect", {Type::String, Type::Int, Type::Int},
      result(Type::Int), Intrinsic::NetTcpConnect);
  add("std.net.tcp_listen", {Type::String, Type::Int, Type::Int},
      result(Type::Int), Intrinsic::NetTcpListen);
  add("std.net.accept", {Type::Int, Type::Int}, result(Type::Int),
      Intrinsic::NetAccept);
  add("std.net.send", {Type::Int, byteBuffer, Type::Int}, result(Type::Int),
      Intrinsic::NetSend);
  add("std.net.receive", {Type::Int, Type::Int, Type::Int}, result(byteBuffer),
      Intrinsic::NetReceive);
  add("std.net.close", {Type::Int}, result(Type::Bool), Intrinsic::NetClose);
  add("std.net.cancel", {Type::Int}, result(Type::Bool), Intrinsic::NetCancel);
  add("std.net.local_port", {Type::Int}, result(Type::Int), Intrinsic::NetLocalPort);

  add("std.http.request", {Type::String, Type::String, byteBuffer, Type::Int},
      result(httpResponse), Intrinsic::HttpRequest);
  add("std.http.read_request", {Type::Int, Type::Int, Type::Int},
      result(httpRequest), Intrinsic::HttpReadRequest);
  add("std.http.write_response",
      {Type::Int, Type::Int, Type::String, byteBuffer, Type::Int},
      result(Type::Bool), Intrinsic::HttpWriteResponse);

  add("std.datetime.format_utc", {Type::Int}, result(Type::String),
      Intrinsic::DateTimeFormatUtc);
  add("std.datetime.parse_utc", {Type::String}, result(Type::Int),
      Intrinsic::DateTimeParseUtc);
  add("std.datetime.days_in_month", {Type::Int, Type::Int}, result(Type::Int),
      Intrinsic::DateTimeDaysInMonth);
  add("std.datetime.weekday", {Type::Int, Type::Int, Type::Int}, result(Type::Int),
      Intrinsic::DateTimeWeekday);
  add("std.datetime.local_offset_minutes", {Type::Int}, result(Type::Int),
      Intrinsic::DateTimeLocalOffsetMinutes);
  add("std.datetime.timezone_name", {}, result(Type::String),
      Intrinsic::DateTimeTimezoneName);
  add("std.log.write", {Type::String, Type::String}, result(Type::Bool),
      Intrinsic::LogWrite);
  add("std.log.append", {Type::String, Type::String, Type::String},
      result(Type::Bool), Intrinsic::LogAppend);
  add("std.cli.has_flag", {arrayType(Type::String), Type::String}, Type::Bool,
      Intrinsic::CliHasFlag);
  add("std.cli.option", {arrayType(Type::String), Type::String},
      result(optionString), Intrinsic::CliOption);
  add("std.cli.positionals", {arrayType(Type::String)}, arrayType(Type::String),
      Intrinsic::CliPositionals);
  add("std.config.get", {Type::String, Type::String}, result(optionString),
      Intrinsic::ConfigGet);
  add("std.config.load", {Type::String, Type::String}, result(optionString),
      Intrinsic::ConfigLoad);
  add("std.compression.xpress_compress", {byteBuffer}, result(byteBuffer),
      Intrinsic::CompressionXpressCompress);
  add("std.compression.xpress_decompress", {byteBuffer}, result(byteBuffer),
      Intrinsic::CompressionXpressDecompress);
  add("std.archive.tar_create",
      {Type::String, arrayType(Type::String), arrayType(byteBuffer)},
      result(Type::Bool), Intrinsic::ArchiveTarCreate);
  add("std.archive.tar_list", {Type::String}, result(arrayType(Type::String)),
      Intrinsic::ArchiveTarList);
  add("std.archive.tar_read", {Type::String, Type::String}, result(byteBuffer),
      Intrinsic::ArchiveTarRead);
  add("std.sqlite.open", {Type::String}, result(Type::Int), Intrinsic::SqliteOpen);
  add("std.sqlite.execute",
      {Type::Int, Type::String, arrayType(Type::String)}, result(Type::Int),
      Intrinsic::SqliteExecute);
  add("std.sqlite.query",
      {Type::Int, Type::String, arrayType(Type::String)}, result(nestedStrings),
      Intrinsic::SqliteQuery);
  add("std.sqlite.close", {Type::Int}, result(Type::Bool), Intrinsic::SqliteClose);
  add("std.testing_core.assert", {Type::Bool, Type::String}, result(Type::Bool),
      Intrinsic::TestingAssert);
  add("std.testing_core.equal_int", {Type::Int, Type::Int, Type::String},
      result(Type::Bool), Intrinsic::TestingEqualInt);
  add("std.testing_core.equal_string", {Type::String, Type::String, Type::String},
      result(Type::Bool), Intrinsic::TestingEqualString);
  add("std.testing_core.temp_directory", {Type::String}, result(Type::String),
      Intrinsic::TestingTempDirectory);
  add("std.testing_core.fixture_path", {Type::String, Type::String},
      result(Type::String), Intrinsic::TestingFixturePath);
  add("std.testing_core.cleanup_temp", {Type::String}, result(Type::Bool),
      Intrinsic::TestingCleanupTemp);
  add("std.testing_core.coverage_hit", {Type::String}, result(Type::Bool),
      Intrinsic::TestingCoverageHit);
  add("std.testing_core.coverage_write", {Type::String}, result(Type::Bool),
      Intrinsic::TestingCoverageWrite);

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

  add("std.target.alias", {}, Type::String, Intrinsic::TargetAlias);
  add("std.target.triple", {}, Type::String, Intrinsic::TargetTriple);
  add("std.target.os", {}, Type::String, Intrinsic::TargetOs);
  add("std.target.architecture", {}, Type::String,
      Intrinsic::TargetArchitecture);
  add("std.target.environment", {}, Type::String,
      Intrinsic::TargetEnvironment);
  add("std.target.pointer_width", {}, Type::Int,
      Intrinsic::TargetPointerWidth);
  add("std.target.endianness", {}, Type::String,
      Intrinsic::TargetEndianness);
  add("std.target.has_feature", {Type::String}, Type::Bool,
      Intrinsic::TargetHasFeature);

  add("std.time.unix_milliseconds", {}, Type::Int, Intrinsic::TimeUnixMilliseconds);
  add("std.time.monotonic_milliseconds", {}, Type::Int,
      Intrinsic::TimeMonotonicMilliseconds);
  add("std.time.sleep_milliseconds", {Type::Int}, Type::Unit,
      Intrinsic::TimeSleepMilliseconds);
  add("std.task.join", {taskType(t)}, result(t), Intrinsic::TaskJoin, {"T"});
  add("std.task.is_complete", {taskType(t)}, Type::Bool,
      Intrinsic::TaskIsComplete, {"T"});
  add("std.task.cancel", {taskType(t)}, Type::Bool, Intrinsic::TaskCancel, {"T"});
  const Type taskGroupT{TypeKind::Struct, "std.task.TaskGroup", {t}};
  add("std.task.group", {arrayType(taskType(t))}, taskGroupT,
      Intrinsic::TaskGroup, {"T"});
  add("std.task.group_join", {taskGroupT}, result(arrayType(t)),
      Intrinsic::TaskGroupJoin, {"T"});
  add("std.task.group_cancel", {taskGroupT}, Type::Bool,
      Intrinsic::TaskGroupCancel, {"T"});
  const Type threadT{TypeKind::Struct, "std.thread.Thread", {t}};
  add("std.thread.spawn", {taskType(t)}, result(threadT), Intrinsic::ThreadSpawn,
      {"T"});
  add("std.thread.join", {threadT}, result(t), Intrinsic::ThreadJoin, {"T"});
  add("std.thread.detach", {threadT}, result(Type::Bool), Intrinsic::ThreadDetach,
      {"T"});
  add("std.thread.is_complete", {threadT}, Type::Bool, Intrinsic::ThreadIsComplete,
      {"T"});
  const Type weakT{TypeKind::Weak, "Weak", {t}};
  add("std.ownership.downgrade", {t}, weakT,
      Intrinsic::OwnershipDowngrade, {"T"});
  add("std.ownership.upgrade", {weakT},
      Type{TypeKind::Enum, "Option", {t}}, Intrinsic::OwnershipUpgrade, {"T"});
  add("std.ownership.expired", {weakT}, Type::Bool,
      Intrinsic::OwnershipExpired, {"T"});
  const Type bufferT{TypeKind::UniqueBuffer, "UniqueBuffer", {t}};
  add("std.buffer.thaw", {arrayType(t)}, bufferT, Intrinsic::BufferThaw, {"T"});
  add("std.buffer.length", {bufferT}, Type::Int, Intrinsic::BufferLength, {"T"});
  add("std.buffer.capacity", {bufferT}, Type::Int, Intrinsic::BufferCapacity, {"T"});
  add("std.buffer.get", {bufferT, Type::Int}, t, Intrinsic::BufferGet, {"T"});
  add("std.buffer.set", {bufferT, Type::Int, t}, bufferT,
      Intrinsic::BufferSet, {"T"});
  add("std.buffer.append", {bufferT, t}, bufferT,
      Intrinsic::BufferAppend, {"T"});
  add("std.buffer.slice", {bufferT, Type::Int, Type::Int}, bufferT,
      Intrinsic::BufferSlice, {"T"});
  add("std.buffer.freeze", {bufferT}, arrayType(t), Intrinsic::BufferFreeze, {"T"});

  const Type cancellation{TypeKind::Struct, "std.cancel.CancellationToken"};
  add("std.cancel.token", {}, cancellation, Intrinsic::CancelToken);
  add("std.cancel.child", {cancellation}, cancellation, Intrinsic::CancelChild);
  add("std.cancel.current", {}, cancellation, Intrinsic::CancelCurrent);
  add("std.cancel.cancel", {cancellation}, Type::Bool, Intrinsic::CancelCancel);
  add("std.cancel.is_cancelled", {cancellation}, Type::Bool,
      Intrinsic::CancelIsCancelled);
  add("std.cancel.check", {cancellation}, result(Type::Bool), Intrinsic::CancelCheck);
  add("std.async_time.deadline_after", {Type::Int}, result(Type::Int),
      Intrinsic::AsyncTimeDeadlineAfter);
  add("std.async_time.remaining", {Type::Int}, Type::Int,
      Intrinsic::AsyncTimeRemaining);
  add("std.async_time.sleep", {Type::Int, cancellation}, taskType(Type::Bool),
      Intrinsic::AsyncTimeSleep);
  add("std.async_time.sleep_until", {Type::Int, cancellation}, taskType(Type::Bool),
      Intrinsic::AsyncTimeSleepUntil);
  const Type charBuffer{TypeKind::UniqueBuffer, "UniqueBuffer", {Type::Char}};
  add("std.async_file.read", {Type::String, Type::Int, cancellation},
      taskType(charBuffer), Intrinsic::AsyncFileRead);
  add("std.async_file.write", {Type::String, charBuffer, Type::Bool, cancellation},
      taskType(Type::Bool), Intrinsic::AsyncFileWrite);
  add("std.async_net.connect",
      {Type::String, Type::Int, Type::Int, cancellation}, taskType(Type::Int),
      Intrinsic::AsyncNetConnect);
  add("std.async_net.accept", {Type::Int, Type::Int, cancellation},
      taskType(Type::Int), Intrinsic::AsyncNetAccept);
  add("std.async_net.receive",
      {Type::Int, Type::Int, Type::Int, cancellation}, taskType(charBuffer),
      Intrinsic::AsyncNetReceive);
  add("std.async_net.send", {Type::Int, charBuffer, Type::Int, cancellation},
      taskType(Type::Int), Intrinsic::AsyncNetSend);
  add("std.async_net.shutdown", {Type::Int}, result(Type::Bool), Intrinsic::NetClose);
  add("std.async_process.run",
      {Type::String, arrayType(Type::String), Type::Int, cancellation},
      taskType(Type::Int), Intrinsic::AsyncProcessRun);

  const Type mutexT{TypeKind::Struct, "std.sync.Mutex", {t}};
  const Type guardT{TypeKind::Struct, "std.sync.LockGuard", {t}};
  const Type event{TypeKind::Struct, "std.sync.Event"};
  const Type atomicInt{TypeKind::Struct, "std.sync.AtomicInt"};
  const Type onceT{TypeKind::Struct, "std.sync.Once", {t}};
  add("std.sync.mutex", {t}, mutexT, Intrinsic::SyncMutex, {"T"});
  add("std.sync.lock", {mutexT, Type::Int, cancellation}, result(guardT),
      Intrinsic::SyncLock, {"T"});
  add("std.sync.guard_get", {guardT}, t, Intrinsic::SyncGuardGet, {"T"});
  add("std.sync.guard_set", {guardT, t}, Type::Bool, Intrinsic::SyncGuardSet, {"T"});
  add("std.sync.unlock", {guardT}, result(Type::Bool), Intrinsic::SyncUnlock, {"T"});
  add("std.sync.event", {Type::Bool, Type::Bool}, event, Intrinsic::SyncEvent);
  add("std.sync.event_set", {event}, Type::Bool, Intrinsic::SyncEventSet);
  add("std.sync.event_reset", {event}, Type::Bool, Intrinsic::SyncEventReset);
  add("std.sync.event_wait", {event, Type::Int, cancellation}, result(Type::Bool),
      Intrinsic::SyncEventWait);
  add("std.sync.atomic_int", {Type::Int}, atomicInt, Intrinsic::SyncAtomicInt);
  add("std.sync.atomic_load", {atomicInt}, Type::Int, Intrinsic::SyncAtomicLoad);
  add("std.sync.atomic_store", {atomicInt, Type::Int}, Type::Unit,
      Intrinsic::SyncAtomicStore);
  add("std.sync.atomic_fetch_add", {atomicInt, Type::Int}, Type::Int,
      Intrinsic::SyncAtomicFetchAdd);
  add("std.sync.atomic_compare_exchange", {atomicInt, Type::Int, Type::Int},
      Type::Bool, Intrinsic::SyncAtomicCompareExchange);
  add("std.sync.once", {t}, onceT, Intrinsic::SyncOnce, {"T"});
  add("std.sync.once_empty", {t}, onceT, Intrinsic::SyncOnceEmpty, {"T"});
  add("std.sync.once_set", {onceT, t}, result(Type::Bool), Intrinsic::SyncOnceSet,
      {"T"});
  add("std.sync.once_get", {onceT}, Type{TypeKind::Enum, "Option", {t}},
      Intrinsic::SyncOnceGet, {"T"});

  const Type channelT{TypeKind::Struct, "std.channel.Channel", {t}};
  const Type senderT{TypeKind::Struct, "std.channel.Sender", {t}};
  const Type receiverT{TypeKind::Struct, "std.channel.Receiver", {t}};
  add("std.channel.bounded", {arrayType(t), Type::Int}, result(channelT),
      Intrinsic::ChannelBounded, {"T"});
  add("std.channel.unbounded", {arrayType(t)}, result(channelT),
      Intrinsic::ChannelUnbounded, {"T"});
  add("std.channel.sender", {channelT}, senderT, Intrinsic::ChannelSender, {"T"});
  add("std.channel.receiver", {channelT}, receiverT, Intrinsic::ChannelReceiver,
      {"T"});
  add("std.channel.clone_sender", {senderT}, senderT, Intrinsic::ChannelCloneSender,
      {"T"});
  add("std.channel.clone_receiver", {receiverT}, receiverT,
      Intrinsic::ChannelCloneReceiver, {"T"});
  add("std.channel.send", {senderT, t, Type::Int, cancellation}, result(Type::Bool),
      Intrinsic::ChannelSend, {"T"});
  add("std.channel.receive", {receiverT, Type::Int, cancellation},
      result(Type{TypeKind::Enum, "Option", {t}}), Intrinsic::ChannelReceive, {"T"});
  add("std.channel.close_sender", {senderT}, result(Type::Bool),
      Intrinsic::ChannelCloseSender, {"T"});
  add("std.channel.close_receiver", {receiverT}, result(Type::Bool),
      Intrinsic::ChannelCloseReceiver, {"T"});
}

void HirLowerer::registerTypeDeclarations() {
  for (const auto& structure : ast_.structs) {
    if (typeDeclarations_.contains(structure.name)) {
      diagnostics_.error(structure.location, "duplicate type '" + structure.name + "'");
      continue;
    }
    const std::uint32_t index = static_cast<std::uint32_t>(hir_.typeDeclarations.size());
    typeDeclarations_.emplace(structure.name, index);
    HirTypeDeclKind kind = HirTypeDeclKind::Struct;
    if (structure.representation == StructRepresentation::Native)
      kind = HirTypeDeclKind::NativeStruct;
    else if (structure.representation == StructRepresentation::Opaque)
      kind = HirTypeDeclKind::Opaque;
    else if (structure.representation == StructRepresentation::Callback)
      kind = HirTypeDeclKind::Callback;
    hir_.typeDeclarations.push_back({kind, structure.name, structure.location,
                                     structure.publicDeclaration, false,
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
    if (structure.representation != StructRepresentation::Rocket &&
        !structure.typeParameters.empty())
      diagnostics_.error(structure.location,
                         "native type declarations cannot be generic");
    std::unordered_set<std::string> fields;
    for (const auto& field : structure.fields) {
      if (!fields.insert(field.name).second)
        diagnostics_.error(field.location, "duplicate field '" + field.name + "'");
      target.fields.push_back({field.name, resolveType(field.typeName, field.location, parameters),
                               field.location});
    }
    if (target.kind == HirTypeDeclKind::NativeStruct) {
      if (target.fields.empty())
        diagnostics_.error(structure.location,
                           "native struct must declare at least one field");
      for (const auto& field : target.fields)
        if (field.type != Type::Int && field.type != Type::Float &&
            field.type != Type::Bool && field.type != Type::Char &&
            field.type.kind != TypeKind::Pointer && field.type.kind != TypeKind::Opaque)
          diagnostics_.error(field.location,
                             "native struct fields must use primitive, Pointer, or opaque types");
    } else if (target.kind == HirTypeDeclKind::Callback) {
      for (const auto& parameter : structure.callbackParameters)
        target.callbackParameters.push_back(
            resolveType(parameter.typeName, parameter.location, parameters));
      target.callbackResult = resolveType(structure.callbackReturnType,
                                          structure.location, parameters);
      for (const auto& parameter : target.callbackParameters)
        if (!isNativeAbiValueType(parameter) || parameter == Type::Unit ||
            parameter.kind == TypeKind::Callback)
          diagnostics_.error(structure.location,
                             "callback parameters must use primitive, Pointer, or opaque types");
      if (!isNativeAbiValueType(target.callbackResult) ||
          target.callbackResult.kind == TypeKind::Callback)
        diagnostics_.error(structure.location,
                           "callback results must use primitive, Pointer, opaque, or Unit types");
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
      HirVariant lowered{variant.name, variant.location, variant.payloadNames, {}};
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
  functionDeclarations_.clear();
  genericFunctions_.clear();
  associatedConstants_.clear();
  nativeConstants_.clear();
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
  activeDefaultExpressions_.clear();
  unsafeDepth_ = 0;
  hir_.library = ast_.library;

  registerBuiltinTypes();
  registerStandardLibrary();
  registerTypeDeclarations();
  registerTraits();

  const SymbolId print = addSymbol(SymbolKind::BuiltinFunction, "print", Type::Unit, false,
                                   {"<builtin>", 1, 1}, {}, Intrinsic::Print);
  hir_.symbols[print].parameterNames = {"value"};
  functions_.emplace("print", print);

  for (const auto& function : ast_.functions) {
    if (function.associatedConstant) associatedConstants_.insert(function.name);
    if (function.nativeConstant) {
      if (!nativeConstants_.emplace(function.name, &function).second)
        diagnostics_.error(function.location,
                           "duplicate native constant '" + function.name + "'");
      functionSymbols_.push_back(InvalidSymbol);
      continue;
    }
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
    if ((function.nativeImport || function.nativeExport) &&
        !function.typeParameters.empty())
      diagnostics_.error(function.location,
                         "native functions cannot be generic");
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
    for (const auto& parameter : function.parameters)
      hir_.symbols[symbol].parameterNames.push_back(parameter.name);
    for (const auto& parameter : function.parameters)
      hir_.symbols[symbol].parameterDefaults.push_back(
          parameter.defaultValue ? parameter.defaultText : std::string());
    hir_.symbols[symbol].nativeImport = function.nativeImport;
    hir_.symbols[symbol].nativeExport = function.nativeExport;
    hir_.symbols[symbol].nativeName = function.nativeName;
    hir_.symbols[symbol].asynchronous = function.asynchronous;
    if (function.asynchronous && !asyncSuccessType(result).has_value())
      diagnostics_.error(function.location,
                         "async function result must be Result[T, String]",
                         DiagnosticCode::AsyncSuspension);
    if (function.asynchronous && (function.nativeImport || function.nativeExport))
      diagnostics_.error(function.location,
                         "async functions cannot be native imports or exports",
                         DiagnosticCode::AsyncSuspension);
    auto validNativeParameter = [](const Type& type) {
      return isNativeAbiValueType(type) && type != Type::Unit &&
             type.kind != TypeKind::NativeStruct;
    };
    if (function.nativeImport || function.nativeExport) {
      for (const auto& parameter : parameters)
        if (!validNativeParameter(parameter))
          diagnostics_.error(function.location,
                             "native function parameters must use primitive, Pointer, opaque, or callback types");
      if (!isNativeAbiValueType(result) || result.kind == TypeKind::NativeStruct ||
          (function.nativeExport && result.kind == TypeKind::Callback))
        diagnostics_.error(function.location,
                           "native function result must use primitive, Pointer, opaque, or Unit type");
    }
    functionSymbols_.push_back(function.nativeImport ? InvalidSymbol : symbol);
    functions_.emplace(function.name, symbol);
    functionDeclarations_.emplace(function.name, &function);
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
  if (!ast_.library && main == functions_.end()) {
    diagnostics_.error({"<module>", 1, 1}, "program must define fn main() -> Int",
                       DiagnosticCode::ControlFlow);
  } else if (!ast_.library) {
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
  if (parsed.kind == TypeKind::Array || parsed.kind == TypeKind::Slice ||
      parsed.kind == TypeKind::Weak || parsed.kind == TypeKind::UniqueBuffer ||
      parsed.kind == TypeKind::Task || parsed.kind == TypeKind::Pointer) {
    Type argument = resolveParsedType(parsed.arguments.at(0), location, substitutions);
    if ((parsed.kind == TypeKind::Array || parsed.kind == TypeKind::Slice) &&
        argument == Type::Unit)
      diagnostics_.error(location, "collections cannot contain Unit values");
    if ((parsed.kind == TypeKind::Array || parsed.kind == TypeKind::Slice) &&
        isNativeType(argument))
      diagnostics_.error(location, "collections cannot contain native values");
    if (parsed.kind == TypeKind::Array) return arrayType(argument);
    if (parsed.kind == TypeKind::Slice) return sliceType(argument);
    if (parsed.kind == TypeKind::Weak) {
      if (!isManagedType(argument))
        diagnostics_.error(location, "Weak target must be a managed type");
      return Type{TypeKind::Weak, "Weak", {argument}};
    }
    if (parsed.kind == TypeKind::UniqueBuffer) {
      if (argument == Type::Unit || isNativeType(argument))
        diagnostics_.error(location,
                           "UniqueBuffer elements cannot be Unit or native values");
      return Type{TypeKind::UniqueBuffer, "UniqueBuffer", {argument}};
    }
    if (parsed.kind == TypeKind::Task) {
      if (isNativeType(argument))
        diagnostics_.error(location, "Task result cannot be a native value");
      return Type{TypeKind::Task, "Task", {argument}};
    }
    return Type{TypeKind::Pointer, "Pointer", {argument}};
  }
  if (parsed.kind != TypeKind::Struct) return parsed;

  const std::unordered_map<std::string, std::size_t> standardConcurrencyTypes{
      {"std.cancel.CancellationToken", 0}, {"std.sync.Event", 0},
      {"std.sync.AtomicInt", 0}, {"std.sync.Mutex", 1},
      {"std.sync.LockGuard", 1}, {"std.sync.Once", 1},
      {"std.channel.Channel", 1}, {"std.channel.Sender", 1},
      {"std.channel.Receiver", 1}, {"std.task.TaskGroup", 1},
      {"std.thread.Thread", 1}};
  if (auto standard = standardConcurrencyTypes.find(parsed.declaration);
      standard != standardConcurrencyTypes.end()) {
    if (parsed.arguments.size() != standard->second) {
      diagnostics_.error(location, "type '" + parsed.declaration + "' expects " +
                                       std::to_string(standard->second) +
                                       " type argument(s)");
      return Type::Invalid;
    }
    std::vector<Type> arguments;
    for (const Type& argument : parsed.arguments)
      arguments.push_back(resolveParsedType(argument, location, substitutions));
    return Type{TypeKind::Struct, parsed.declaration, std::move(arguments)};
  }

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
  TypeKind kind = TypeKind::Struct;
  if (declaration.kind == HirTypeDeclKind::Enum) kind = TypeKind::Enum;
  else if (declaration.kind == HirTypeDeclKind::NativeStruct) kind = TypeKind::NativeStruct;
  else if (declaration.kind == HirTypeDeclKind::Opaque) kind = TypeKind::Opaque;
  else if (declaration.kind == HirTypeDeclKind::Callback) kind = TypeKind::Callback;
  return Type{kind, declaration.name, std::move(arguments)};
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
  unsafeDepth_ = 0;
  currentAsync_ = function.asynchronous;
  movedSymbols_.clear();
  borrowUniqueDepth_ = 0;

  HirFunction result;
  result.symbol = symbol;
  result.location = function.location;
  result.result = signature.type;
  result.asynchronous = function.asynchronous;

  std::unordered_set<std::string> names;
  for (std::size_t index = 0; index < function.parameters.size(); ++index) {
    const auto& parameter = function.parameters[index];
    const Type type = signature.parameterTypes[index];
    if (parameter.defaultValue) {
      activeDefaultExpressions_.insert(parameter.defaultValue.get());
      auto value = lowerExpression(*parameter.defaultValue, type);
      activeDefaultExpressions_.erase(parameter.defaultValue.get());
      if (value->type != Type::Invalid && value->type != type)
        diagnostics_.error(parameter.defaultValue->location,
                           "default argument for '" + parameter.name + "' has type " +
                               typeName(value->type) + ", expected " + typeName(type));
    }
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
  unsafeDepth_ = 0;
  currentAsync_ = specialization.function->asynchronous;
  movedSymbols_.clear();
  borrowUniqueDepth_ = 0;

  HirFunction result;
  result.symbol = specialization.symbol;
  result.location = specialization.function->location;
  result.result = specialization.result;
  result.asynchronous = specialization.function->asynchronous;
  std::unordered_set<std::string> names;
  for (std::size_t index = 0; index < specialization.function->parameters.size(); ++index) {
    const auto& parameter = specialization.function->parameters[index];
    const Type type = specialization.parameters[index];
    if (parameter.defaultValue) {
      activeDefaultExpressions_.insert(parameter.defaultValue.get());
      auto value = lowerExpression(*parameter.defaultValue, type);
      activeDefaultExpressions_.erase(parameter.defaultValue.get());
      if (value->type != Type::Invalid && value->type != type)
        diagnostics_.error(parameter.defaultValue->location,
                           "default argument for '" + parameter.name + "' has type " +
                               typeName(value->type) + ", expected " + typeName(type));
    }
    const SymbolId parameterSymbol = addSymbol(SymbolKind::Parameter, parameter.name,
                                                type, false,
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
    if (actual.declaration == "std.sync.LockGuard" ||
        actual.declaration == "std.task.TaskGroup")
      diagnostics_.error(returned.location,
                         "scoped concurrency value cannot escape its function",
                         DiagnosticCode::ScopedLifetime);
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
  case StmtKind::Unsafe: {
    const auto& unsafe = static_cast<const UnsafeStmt&>(statement);
    ++unsafeDepth_;
    auto body = lowerBlock(unsafe.body, returnType, true);
    --unsafeDepth_;
    return std::make_unique<HirUnsafeStmt>(unsafe.location, std::move(body));
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
  for (const auto& parameter : function.parameters)
    hir_.symbols[symbol].parameterNames.push_back(parameter.name);
  for (const auto& parameter : function.parameters)
    hir_.symbols[symbol].parameterDefaults.push_back(
        parameter.defaultValue ? parameter.defaultText : std::string());
  hir_.symbols[symbol].asynchronous = function.asynchronous;
  if (function.asynchronous && !asyncSuccessType(result).has_value())
    diagnostics_.error(location,
                       "async function result must be Result[T, String]",
                       DiagnosticCode::AsyncSuspension);
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
  case ExprKind::NamedArgument:
    collectLambdaCaptures(*static_cast<const NamedArgumentExpr&>(expression).value,
                          parameters, captures);
    break;
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
  case ExprKind::Await:
    collectLambdaCaptures(*static_cast<const AwaitExpr&>(expression).value,
                          parameters, captures); break;
  case ExprKind::Lambda: break;
  default: break;
  }
}

HirFunction HirLowerer::lowerLambda(const PendingLambda& pending) {
  currentSubstitutions_ = pending.substitutions;
  currentReturnType_ = hir_.symbol(pending.symbol).type;
  scopes_.clear();
  scopes_.emplace_back();
  activeCaptures_.clear();
  loopDepth_ = 0;
  unsafeDepth_ = 0;
  currentAsync_ = false;
  movedSymbols_.clear();
  borrowUniqueDepth_ = 0;

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
    const Type type = resolveType(parameter.typeName, parameter.location,
                                  currentSubstitutions_);
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

void HirLowerer::fillDefaultArguments(
    const Function& function, const Location& location,
    std::vector<std::unique_ptr<HirExpr>>& arguments,
    std::vector<std::size_t>& evaluationOrder) {
  if (arguments.size() > function.parameters.size()) return;
  arguments.resize(function.parameters.size());

  Substitutions markers;
  for (const auto& parameter : function.typeParameters)
    markers.emplace(parameter, typeParameter(parameter));
  std::vector<Type> patterns;
  patterns.reserve(function.parameters.size());
  for (const auto& parameter : function.parameters)
    patterns.push_back(resolveType(parameter.typeName, parameter.location, markers));

  Substitutions inferred;
  if (!function.typeParameters.empty()) {
    for (std::size_t index = 0; index < arguments.size(); ++index)
      if (arguments[index] && arguments[index]->type != Type::Invalid)
        (void)inferTypeArguments(patterns[index], arguments[index]->type, inferred,
                                 arguments[index]->location);
  }

  for (std::size_t index = 0; index < function.parameters.size(); ++index) {
    if (arguments[index]) continue;
    const Parameter& parameter = function.parameters[index];
    if (!parameter.defaultValue) {
      diagnostics_.error(location,
                         "missing required argument '" + parameter.name + "'",
                         DiagnosticCode::Arity);
      arguments[index] =
          std::make_unique<HirLiteralExpr>(location, Type::Invalid, "0");
      continue;
    }
    if (activeDefaultExpressions_.contains(parameter.defaultValue.get())) {
      diagnostics_.error(parameter.defaultValue->location,
                         "cyclic default argument expansion for '" +
                             parameter.name + "'",
                         DiagnosticCode::Arity);
      arguments[index] = std::make_unique<HirLiteralExpr>(
          parameter.defaultValue->location, Type::Invalid, "0");
      evaluationOrder.push_back(index);
      continue;
    }

    Scope declarationScope;
    std::vector<HirDefaultArgumentBinding> bindings;
    for (std::size_t earlier = 0; earlier < index; ++earlier) {
      Type bindingType = arguments[earlier]
                             ? arguments[earlier]->type
                             : substitute(patterns[earlier], inferred);
      const SymbolId symbol = addSymbol(
          SymbolKind::Parameter,
          "$default." + function.name + "." + std::to_string(index) + "." +
              function.parameters[earlier].name,
          bindingType, false, function.parameters[earlier].location);
      declarationScope.emplace(function.parameters[earlier].name, symbol);
      bindings.push_back({earlier, symbol});
    }

    auto savedScopes = std::move(scopes_);
    auto savedSubstitutions = std::move(currentSubstitutions_);
    auto savedCaptures = std::move(activeCaptures_);
    auto savedMoved = std::move(movedSymbols_);
    const Type savedReturn = currentReturnType_;
    const int savedLoopDepth = loopDepth_;
    const int savedUnsafeDepth = unsafeDepth_;
    const bool savedAsync = currentAsync_;
    const int savedBorrowDepth = borrowUniqueDepth_;

    scopes_.clear();
    scopes_.push_back(std::move(declarationScope));
    currentSubstitutions_ = inferred;
    currentReturnType_ = substitute(
        resolveType(function.returnType, function.location, markers), inferred);
    loopDepth_ = 0;
    unsafeDepth_ = 0;
    currentAsync_ = function.asynchronous;
    borrowUniqueDepth_ = 0;
    activeDefaultExpressions_.insert(parameter.defaultValue.get());
    const Type expectedType = substitute(patterns[index], inferred);
    auto value = lowerExpression(
        *parameter.defaultValue,
        containsTypeParameter(expectedType) ? std::nullopt
                                            : std::optional<Type>(expectedType));
    activeDefaultExpressions_.erase(parameter.defaultValue.get());

    scopes_ = std::move(savedScopes);
    currentSubstitutions_ = std::move(savedSubstitutions);
    activeCaptures_ = std::move(savedCaptures);
    movedSymbols_ = std::move(savedMoved);
    currentReturnType_ = savedReturn;
    loopDepth_ = savedLoopDepth;
    unsafeDepth_ = savedUnsafeDepth;
    currentAsync_ = savedAsync;
    borrowUniqueDepth_ = savedBorrowDepth;

    bool validType = true;
    if (function.typeParameters.empty()) {
      validType = value->type == Type::Invalid || value->type == expectedType;
    } else if (value->type != Type::Invalid) {
      validType = inferTypeArguments(patterns[index], value->type, inferred,
                                     parameter.defaultValue->location);
    }
    if (!validType)
      diagnostics_.error(parameter.defaultValue->location,
                         "default argument for '" + parameter.name + "' has type " +
                             typeName(value->type) + ", expected " +
                             typeName(substitute(patterns[index], inferred)));
    arguments[index] = std::make_unique<HirDefaultArgumentExpr>(
        parameter.defaultValue->location, value->type, std::move(value),
        std::move(bindings));
    evaluationOrder.push_back(index);
  }
}

std::unique_ptr<HirExpr> HirLowerer::lowerResolvedCall(
    const std::string& name, const Location& location,
    std::vector<std::unique_ptr<HirExpr>> arguments,
    std::vector<std::size_t> argumentOrder) {
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
        diagnostics_.error(
            arguments[index]->location,
            "argument '" + definition.parameterNames[index] + "' has type " +
                typeName(arguments[index]->type) + ", expected " +
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
    if (definition.intrinsic == Intrinsic::OwnershipDowngrade &&
        inferred.contains("T") &&
        (weakType(inferred.at("T")) == Type::Invalid ||
         !isShareType(inferred.at("T"))))
      diagnostics_.error(location,
                         "Weak targets must be identity-bearing Share values",
                         DiagnosticCode::ShareConstraint);
    SymbolId callee = InvalidSymbol;
    if (auto found = specializations_.find(key); found != specializations_.end()) {
      callee = found->second;
    } else {
      callee = addSymbol(SymbolKind::BuiltinFunction, key, result, false,
                         {"<standard-library>", 1, 1}, parameterTypes,
                         definition.intrinsic);
      hir_.symbols[callee].parameterNames = definition.parameterNames;
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
                                         std::move(arguments),
                                         std::move(argumentOrder));
  }
  if (auto generic = genericFunctions_.find(name); generic != genericFunctions_.end()) {
    std::vector<std::size_t> evaluationOrder;
    for (std::size_t index = 0; index < arguments.size(); ++index)
      evaluationOrder.push_back(index);
    fillDefaultArguments(*generic->second, location, arguments, evaluationOrder);
    const SymbolId callee = specializeFunction(*generic->second, arguments, location);
    if (callee != InvalidSymbol && hir_.symbol(callee).asynchronous) {
      const auto success = asyncSuccessType(hir_.symbol(callee).type);
      diagnoseAsyncArguments(arguments, location);
      if (success.has_value() && !isSendType(*success))
        diagnostics_.error(location,
                           "async task result type " + typeName(*success) +
                               " does not satisfy Send",
                           DiagnosticCode::SendConstraint);
      return std::make_unique<HirAsyncCallExpr>(
          location, success.has_value() ? taskType(*success) : Type::Invalid,
          callee, std::move(arguments), std::move(evaluationOrder));
    }
    const Type result = callee == InvalidSymbol ? Type::Invalid : hir_.symbol(callee).type;
    return std::make_unique<HirCallExpr>(location, result, callee,
                                        std::move(arguments), std::move(evaluationOrder));
  }

  auto found = functions_.find(name);
  if (found == functions_.end()) {
    diagnostics_.error(location, "unknown method '" + name + "'", DiagnosticCode::Name);
    return std::make_unique<HirCallExpr>(location, Type::Invalid, InvalidSymbol,
                                         std::move(arguments));
  }
  const SymbolId callee = found->second;
  const HirSymbol signature = hir_.symbol(callee);
  if (signature.nativeImport && unsafeDepth_ == 0)
    diagnostics_.error(location,
                       "call to extern function '" + name +
                           "' requires an explicit unsafe block");
  std::vector<std::size_t> evaluationOrder;
  for (std::size_t index = 0; index < arguments.size(); ++index)
    evaluationOrder.push_back(index);
  if (auto declaration = functionDeclarations_.find(name);
      declaration != functionDeclarations_.end())
    fillDefaultArguments(*declaration->second, location, arguments, evaluationOrder);
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
  if (signature.asynchronous) {
    const auto success = asyncSuccessType(signature.type);
    diagnoseAsyncArguments(arguments, location);
    if (success.has_value() && !isSendType(*success))
      diagnostics_.error(location,
                         "async task result type " + typeName(*success) +
                             " does not satisfy Send",
                         DiagnosticCode::SendConstraint);
    return std::make_unique<HirAsyncCallExpr>(
        location, success.has_value() ? taskType(*success) : Type::Invalid,
        callee, std::move(arguments), std::move(evaluationOrder));
  }
  return std::make_unique<HirCallExpr>(location, signature.type, callee,
                                       std::move(arguments), std::move(evaluationOrder));
}

std::unique_ptr<HirExpr> HirLowerer::lowerNamedStandardCall(
    const std::string& name, const Location& location,
    std::vector<std::unique_ptr<HirExpr>> leadingArguments,
    const std::vector<std::unique_ptr<Expr>>& sourceArguments) {
  const auto found = standardFunctions_.find(name);
  if (found == standardFunctions_.end())
    return lowerResolvedCall(name, location, std::move(leadingArguments));
  const StandardFunction& definition = found->second;
  const std::size_t leadingCount = leadingArguments.size();
  if (definition.parameterNames.size() != definition.parameterTypes.size() ||
      definition.parameterNames.size() < leadingCount) {
    diagnostics_.error(location,
                       "named argument metadata is unavailable for '" + name + "'",
                       DiagnosticCode::Arity);
    return std::make_unique<HirCallExpr>(location, Type::Invalid, InvalidSymbol,
                                         std::move(leadingArguments));
  }
  std::vector<std::string> explicitNames(
      definition.parameterNames.begin() + leadingCount,
      definition.parameterNames.end());
  const NamedArgumentBinding binding =
      bindNamedArguments(sourceArguments, explicitNames, location, diagnostics_);
  std::vector<std::unique_ptr<HirExpr>> arguments(definition.parameterTypes.size());
  std::vector<std::size_t> evaluationOrder;
  for (std::size_t index = 0; index < leadingCount; ++index) {
    arguments[index] = std::move(leadingArguments[index]);
    evaluationOrder.push_back(index);
  }
  for (std::size_t source = 0; source < sourceArguments.size(); ++source) {
    if (!binding.sourceToParameter[source].has_value()) {
      (void)lowerExpression(callArgumentValue(*sourceArguments[source]));
      continue;
    }
    const std::size_t parameter = leadingCount + *binding.sourceToParameter[source];
    std::optional<Type> expected;
    if (!containsTypeParameter(definition.parameterTypes[parameter]))
      expected = definition.parameterTypes[parameter];
    const bool borrowsUnique = parameter == 0 &&
        (definition.intrinsic == Intrinsic::BufferLength ||
         definition.intrinsic == Intrinsic::BufferCapacity ||
         definition.intrinsic == Intrinsic::BufferGet ||
         definition.intrinsic == Intrinsic::SyncGuardGet ||
         definition.intrinsic == Intrinsic::SyncGuardSet ||
         definition.intrinsic == Intrinsic::TaskIsComplete ||
         definition.intrinsic == Intrinsic::TaskCancel ||
         definition.intrinsic == Intrinsic::TaskGroupCancel ||
         definition.intrinsic == Intrinsic::ThreadIsComplete);
    if (borrowsUnique) ++borrowUniqueDepth_;
    arguments[parameter] =
        lowerExpression(callArgumentValue(*sourceArguments[source]), expected);
    if (borrowsUnique) --borrowUniqueDepth_;
    evaluationOrder.push_back(parameter);
  }
  for (auto& argument : arguments)
    if (!argument)
      argument =
          std::make_unique<HirLiteralExpr>(location, Type::Invalid, "0");
  return lowerResolvedCall(name, location, std::move(arguments),
                           std::move(evaluationOrder));
}

std::unique_ptr<HirExpr> HirLowerer::lowerNamedUserCall(
    const std::string& name, const Location& location,
    std::vector<std::unique_ptr<HirExpr>> leadingArguments,
    const std::vector<std::unique_ptr<Expr>>& sourceArguments) {
  const Function* genericDeclaration = nullptr;
  const Function* declaration = nullptr;
  const HirSymbol* signature = nullptr;
  if (auto generic = genericFunctions_.find(name); generic != genericFunctions_.end()) {
    genericDeclaration = generic->second;
    declaration = generic->second;
  } else if (auto found = functions_.find(name); found != functions_.end()) {
    signature = &hir_.symbol(found->second);
    if (auto source = functionDeclarations_.find(name);
        source != functionDeclarations_.end())
      declaration = source->second;
  } else {
    diagnostics_.error(location, "unknown method '" + name + "'", DiagnosticCode::Name);
    return std::make_unique<HirCallExpr>(location, Type::Invalid, InvalidSymbol,
                                         std::move(leadingArguments));
  }

  std::vector<std::string> allNames;
  std::vector<Type> parameterTypes;
  if (genericDeclaration) {
    for (const auto& parameter : genericDeclaration->parameters)
      allNames.push_back(parameter.name);
  } else {
    allNames = signature->parameterNames;
    parameterTypes = signature->parameterTypes;
  }
  const std::size_t leadingCount = leadingArguments.size();
  if (allNames.size() < leadingCount) {
    diagnostics_.error(location, "named argument metadata is unavailable for '" + name + "'",
                       DiagnosticCode::Arity);
    return std::make_unique<HirCallExpr>(location, Type::Invalid, InvalidSymbol,
                                         std::move(leadingArguments));
  }
  std::vector<std::string> explicitNames(allNames.begin() + leadingCount, allNames.end());
  std::vector<bool> explicitDefaults(explicitNames.size(), false);
  if (declaration)
    for (std::size_t index = leadingCount; index < declaration->parameters.size(); ++index)
      explicitDefaults[index - leadingCount] =
          declaration->parameters[index].defaultValue != nullptr;
  const NamedArgumentBinding binding =
      bindNamedArguments(sourceArguments, explicitNames, location, diagnostics_,
                         explicitDefaults);
  std::vector<std::unique_ptr<HirExpr>> arguments(allNames.size());
  std::vector<std::size_t> evaluationOrder;
  for (std::size_t index = 0; index < leadingCount; ++index) {
    arguments[index] = std::move(leadingArguments[index]);
    evaluationOrder.push_back(index);
  }
  for (std::size_t source = 0; source < sourceArguments.size(); ++source) {
    if (!binding.sourceToParameter[source].has_value()) {
      (void)lowerExpression(callArgumentValue(*sourceArguments[source]));
      continue;
    }
    const std::size_t parameter = leadingCount + *binding.sourceToParameter[source];
    std::optional<Type> expected;
    if (!genericDeclaration && parameter < parameterTypes.size())
      expected = parameterTypes[parameter];
    arguments[parameter] = lowerExpression(callArgumentValue(*sourceArguments[source]), expected);
    evaluationOrder.push_back(parameter);
  }
  for (std::size_t index = 0; index < arguments.size(); ++index)
    if (!arguments[index] &&
        (!declaration || !declaration->parameters[index].defaultValue))
      arguments[index] =
          std::make_unique<HirLiteralExpr>(location, Type::Invalid, "0");
  if (declaration)
    fillDefaultArguments(*declaration, location, arguments, evaluationOrder);

  SymbolId callee = InvalidSymbol;
  if (genericDeclaration)
    callee = specializeFunction(*genericDeclaration, arguments, location);
  else
    callee = signature->id;
  const HirSymbol* resolved = callee == InvalidSymbol ? nullptr : &hir_.symbol(callee);
  if (resolved && resolved->nativeImport && unsafeDepth_ == 0)
    diagnostics_.error(location, "call to extern function '" + name +
                                     "' requires an explicit unsafe block");
  if (resolved) {
    for (std::size_t index = 0;
         index < arguments.size() && index < resolved->parameterTypes.size(); ++index)
      if (arguments[index]->type != Type::Invalid &&
          arguments[index]->type != resolved->parameterTypes[index])
        diagnostics_.error(arguments[index]->location,
                           "argument '" + allNames[index] + "' has type " +
                               typeName(arguments[index]->type) + ", expected " +
                               typeName(resolved->parameterTypes[index]));
  }
  if (resolved && resolved->asynchronous) {
    const auto success = asyncSuccessType(resolved->type);
    diagnoseAsyncArguments(arguments, location);
    if (success.has_value() && !isSendType(*success))
      diagnostics_.error(location, "async task result type " + typeName(*success) +
                                       " does not satisfy Send",
                         DiagnosticCode::SendConstraint);
    return std::make_unique<HirAsyncCallExpr>(
        location, success.has_value() ? taskType(*success) : Type::Invalid,
        callee, std::move(arguments), std::move(evaluationOrder));
  }
  return std::make_unique<HirCallExpr>(
      location, resolved ? resolved->type : Type::Invalid, callee,
      std::move(arguments), std::move(evaluationOrder));
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
    if (expected.has_value() && expected->kind == TypeKind::Callback) {
      auto callback = typeDeclarations_.find(expected->declaration);
      auto target = functions_.find(name);
      if (callback == typeDeclarations_.end() || target == functions_.end()) {
        diagnostics_.error(expression.location,
                           "callback value must name a compatible top-level Rocket function");
        return std::make_unique<HirFunctionRefExpr>(expression.location,
                                                    *expected, InvalidSymbol);
      }
      const auto& callbackType = hir_.typeDeclarations[callback->second];
      const auto& function = hir_.symbol(target->second);
      if (function.nativeImport || function.parameterTypes != callbackType.callbackParameters ||
          function.type != callbackType.callbackResult)
        diagnostics_.error(expression.location,
                           "function '" + name + "' does not match callback type " +
                               typeName(*expected));
      return std::make_unique<HirFunctionRefExpr>(expression.location, *expected,
                                                  target->second);
    }
    if (auto constant = nativeConstants_.find(name);
        constant != nativeConstants_.end()) {
      const Function& declaration = *constant->second;
      const Type declared = resolveType(declaration.returnType, declaration.location);
      if (declaration.body.size() != 1 ||
          declaration.body.front()->kind != StmtKind::Return) {
        diagnostics_.error(declaration.location,
                           "native constant requires one literal initializer");
        return std::make_unique<HirLiteralExpr>(expression.location,
                                                Type::Invalid, "0");
      }
      const auto& returned = static_cast<const ReturnStmt&>(*declaration.body.front());
      bool primitiveLiteral = returned.value &&
          (returned.value->kind == ExprKind::Integer ||
           returned.value->kind == ExprKind::Float ||
           returned.value->kind == ExprKind::Character ||
           returned.value->kind == ExprKind::Bool);
      if (returned.value && returned.value->kind == ExprKind::Unary) {
        const auto& unary = static_cast<const UnaryExpr&>(*returned.value);
        primitiveLiteral = unary.op == TokenKind::Minus && unary.operand &&
            (unary.operand->kind == ExprKind::Integer ||
             unary.operand->kind == ExprKind::Float);
      }
      if (!primitiveLiteral)
        diagnostics_.error(declaration.location,
                           "native constant initializer must be a primitive literal");
      auto value = returned.value
                       ? lowerExpression(*returned.value, declared)
                       : std::make_unique<HirLiteralExpr>(expression.location,
                                                          Type::Invalid, "0");
      if (value->type != Type::Invalid && value->type != declared)
        diagnostics_.error(declaration.location,
                           "native constant initializer type does not match " +
                               typeName(declared));
      return value;
    }
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
    const Type& symbolType = hir_.symbol(symbol).type;
    const bool moveOnly = isMoveOnlyType(symbolType);
    if (moveOnly) {
      if (movedSymbols_.contains(symbol))
        diagnostics_.error(expression.location,
                           "move-only value '" + name + "' was already consumed",
                           DiagnosticCode::MoveOnly);
      else if (borrowUniqueDepth_ == 0)
        movedSymbols_.insert(symbol);
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
    const bool namedArguments = hasNamedArguments(call.arguments);
    auto lowerWrittenArguments = [&]() {
      std::vector<std::unique_ptr<HirExpr>> result;
      for (const auto& argument : call.arguments)
        result.push_back(lowerExpression(callArgumentValue(*argument)));
      return result;
    };
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
          if (namedArguments)
            return lowerNamedUserCall(closureType.declaration + ".call",
                                      expression.location, std::move(arguments),
                                      call.arguments);
          for (const auto& argument : call.arguments)
            arguments.push_back(lowerExpression(callArgumentValue(*argument)));
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
      if (namedArguments)
        return lowerNamedUserCall(closureType.declaration + ".call",
                                  expression.location, std::move(arguments),
                                  call.arguments);
      for (const auto& argument : call.arguments)
        arguments.push_back(lowerExpression(callArgumentValue(*argument)));
      return lowerResolvedCall(closureType.declaration + ".call",
                               expression.location, std::move(arguments));
    }
    if (call.callee->kind == ExprKind::Field) {
      const auto& field = static_cast<const FieldExpr&>(*call.callee);
      if (field.value->kind == ExprKind::Name) {
        const std::string owner = static_cast<const LiteralExpr&>(*field.value).value;
        const std::string associated = owner + "." + field.field;
        if (typeDeclarations_.contains(owner) || owner == "String") {
          const std::string target = associatedLibraryFunction(associated);
          if (namedArguments) {
            if (standardFunctions_.contains(target))
              return lowerNamedStandardCall(target, expression.location, {},
                                            call.arguments);
            return lowerNamedUserCall(target, expression.location, {}, call.arguments);
          }
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
      if (namedArguments) {
        if (standardFunctions_.contains(target)) {
          std::vector<std::unique_ptr<HirExpr>> leading;
          leading.push_back(std::move(receiver));
          return lowerNamedStandardCall(target, expression.location,
                                        std::move(leading), call.arguments);
        }
        std::vector<std::unique_ptr<HirExpr>> leading;
        leading.push_back(std::move(receiver));
        return lowerNamedUserCall(target, expression.location, std::move(leading),
                                  call.arguments);
      }
      arguments.push_back(std::move(receiver));
      for (const auto& argument : call.arguments)
        arguments.push_back(lowerExpression(*argument));
      return lowerResolvedCall(target, expression.location, std::move(arguments));
    }
    if (call.callee->kind != ExprKind::Name) {
      auto arguments = lowerWrittenArguments();
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
      const auto& variant = declaration.variants[tag];
      if (namedArguments && !structConstructor && variant.payloadNames.empty()) {
        diagnostics_.error(expression.location,
                           "named arguments are not supported for anonymous enum payloads",
                           DiagnosticCode::Arity);
        return std::make_unique<HirAggregateExpr>(
            expression.location, Type::Invalid, *aggregateDeclaration, tag,
            lowerWrittenArguments());
      }
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
      std::vector<std::size_t> argumentOrder;
      NamedArgumentBinding binding;
      std::vector<std::string> parameterNames;
      if (namedArguments) {
        if (structConstructor)
          for (const auto& field : declaration.fields)
            parameterNames.push_back(field.name);
        else
          parameterNames = variant.payloadNames;
        binding = bindNamedArguments(call.arguments, parameterNames, expression.location,
                                     diagnostics_);
        arguments.resize(patterns.size());
      }
      for (std::size_t source = 0; source < call.arguments.size(); ++source) {
        const std::size_t index = namedArguments && binding.sourceToParameter[source].has_value()
                                      ? *binding.sourceToParameter[source]
                                      : source;
        if (namedArguments && !binding.sourceToParameter[source].has_value()) {
          (void)lowerExpression(callArgumentValue(*call.arguments[source]));
          continue;
        }
        std::optional<Type> argumentExpected;
        if (index < patterns.size()) {
          Type candidate = substitute(patterns[index], substitutions);
          if (!containsTypeParameter(candidate)) argumentExpected = candidate;
        }
        auto lowered = lowerExpression(callArgumentValue(*call.arguments[source]), argumentExpected);
        if (namedArguments) {
          arguments[index] = std::move(lowered);
          argumentOrder.push_back(index);
        } else {
          arguments.push_back(std::move(lowered));
        }
        if (index < patterns.size())
          inferTypeArguments(patterns[index], arguments[index]->type, substitutions,
                             arguments[index]->location);
      }
      for (auto& argument : arguments)
        if (!argument)
          argument = std::make_unique<HirLiteralExpr>(expression.location, Type::Invalid, "0");
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
          diagnostics_.error(arguments[index]->location,
                             namedArguments
                                 ? "argument '" + parameterNames[index] +
                                       "' has type " + typeName(arguments[index]->type) +
                                       ", expected " + typeName(required)
                                 : "constructor argument type is " +
                                       typeName(arguments[index]->type) + ", expected " +
                                       typeName(required));
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
                                                 std::move(arguments),
                                                 std::move(argumentOrder));
    }

    auto standard = standardFunctions_.find(name);
    if (standard != standardFunctions_.end()) {
      if (namedArguments)
        return lowerNamedStandardCall(name, expression.location, {}, call.arguments);
      const StandardFunction& definition = standard->second;
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (std::size_t index = 0; index < call.arguments.size(); ++index) {
        std::optional<Type> argumentExpected;
        if (index < definition.parameterTypes.size() &&
            !containsTypeParameter(definition.parameterTypes[index]))
          argumentExpected = definition.parameterTypes[index];
        const bool borrowsBuffer = index == 0 &&
            (definition.intrinsic == Intrinsic::BufferLength ||
             definition.intrinsic == Intrinsic::BufferCapacity ||
             definition.intrinsic == Intrinsic::BufferGet ||
             definition.intrinsic == Intrinsic::SyncGuardGet ||
             definition.intrinsic == Intrinsic::SyncGuardSet ||
             definition.intrinsic == Intrinsic::TaskIsComplete ||
             definition.intrinsic == Intrinsic::TaskCancel ||
             definition.intrinsic == Intrinsic::TaskGroupCancel ||
             definition.intrinsic == Intrinsic::ThreadIsComplete);
        if (borrowsBuffer) ++borrowUniqueDepth_;
        arguments.push_back(lowerExpression(*call.arguments[index], argumentExpected));
        if (borrowsBuffer) --borrowUniqueDepth_;
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
      if (definition.intrinsic == Intrinsic::OwnershipDowngrade &&
          inferred.contains("T") &&
          (weakType(inferred.at("T")) == Type::Invalid ||
           !isShareType(inferred.at("T"))))
        diagnostics_.error(expression.location,
                           "Weak targets must be identity-bearing Share values",
                           DiagnosticCode::ShareConstraint);
      const bool concurrencyValue =
          definition.intrinsic == Intrinsic::SyncMutex ||
          definition.intrinsic == Intrinsic::SyncOnce ||
          definition.intrinsic == Intrinsic::SyncOnceEmpty ||
          definition.intrinsic == Intrinsic::ChannelBounded ||
          definition.intrinsic == Intrinsic::ChannelUnbounded ||
          definition.intrinsic == Intrinsic::ChannelSend ||
          definition.intrinsic == Intrinsic::TaskGroup ||
          definition.intrinsic == Intrinsic::ThreadSpawn;
      if (concurrencyValue && inferred.contains("T")) {
        const Type& transferred = inferred.at("T");
        const bool shareValue =
            definition.intrinsic == Intrinsic::SyncMutex ||
            definition.intrinsic == Intrinsic::SyncOnce ||
            definition.intrinsic == Intrinsic::SyncOnceEmpty;
        if ((shareValue && !isShareType(transferred)) ||
            (!shareValue && !isSendType(transferred)))
          diagnostics_.error(expression.location,
                             "concurrency boundary type " + typeName(transferred) +
                                 (shareValue ? " does not satisfy Share"
                                            : " does not satisfy Send"),
                             DiagnosticCode::SendConstraint);
      }
      if (definition.intrinsic == Intrinsic::BufferThaw &&
          inferred.contains("T") && !isShareType(inferred.at("T")))
        diagnostics_.error(expression.location,
                           "UniqueBuffer element type " + typeName(inferred.at("T")) +
                               " does not satisfy Share",
                           DiagnosticCode::SendConstraint);
      SymbolId callee = InvalidSymbol;
      if (auto found = specializations_.find(key); found != specializations_.end()) {
        callee = found->second;
      } else {
        callee = addSymbol(SymbolKind::BuiltinFunction, key, result, false,
                           {"<standard-library>", 1, 1}, parameterTypes,
                           definition.intrinsic);
        hir_.symbols[callee].parameterNames = definition.parameterNames;
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
      if (namedArguments)
        return lowerNamedUserCall(name, expression.location, {}, call.arguments);
      std::vector<std::unique_ptr<HirExpr>> arguments;
      for (const auto& argument : call.arguments) arguments.push_back(lowerExpression(*argument));
      return lowerResolvedCall(name, expression.location, std::move(arguments));
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
    if (namedArguments && signature.kind != SymbolKind::BuiltinFunction)
      return lowerNamedUserCall(name, expression.location, {}, call.arguments);
    std::vector<std::unique_ptr<HirExpr>> arguments;
    std::vector<std::size_t> argumentOrder;
    NamedArgumentBinding builtinBinding;
    if (signature.kind == SymbolKind::BuiltinFunction) {
      builtinBinding = bindNamedArguments(call.arguments, signature.parameterNames,
                                          expression.location, diagnostics_);
      arguments.resize(signature.parameterNames.size());
    }
    for (std::size_t source = 0; source < call.arguments.size(); ++source) {
      if (signature.kind == SymbolKind::BuiltinFunction &&
          !builtinBinding.sourceToParameter[source].has_value()) {
        (void)lowerExpression(callArgumentValue(*call.arguments[source]));
        continue;
      }
      const std::size_t index =
          signature.kind == SymbolKind::BuiltinFunction
              ? *builtinBinding.sourceToParameter[source]
              : source;
      std::optional<Type> argumentExpected;
      if (signature.kind != SymbolKind::BuiltinFunction &&
          index < signature.parameterTypes.size())
        argumentExpected = signature.parameterTypes[index];
      auto lowered = lowerExpression(callArgumentValue(*call.arguments[source]),
                                     argumentExpected);
      if (signature.kind == SymbolKind::BuiltinFunction) {
        arguments[index] = std::move(lowered);
        argumentOrder.push_back(index);
      } else {
        arguments.push_back(std::move(lowered));
      }
    }
    for (auto& argument : arguments)
      if (!argument)
        argument =
            std::make_unique<HirLiteralExpr>(expression.location, Type::Invalid, "0");
    if (signature.kind == SymbolKind::BuiltinFunction) {
      if (isCollectionType(arguments[0]->type) || isAggregateType(arguments[0]->type) ||
               isNativeType(arguments[0]->type))
        diagnostics_.error(arguments[0]->location,
                           "print does not accept aggregate or native values");
      return std::make_unique<HirCallExpr>(expression.location, Type::Unit, callee,
                                           std::move(arguments),
                                           std::move(argumentOrder));
    }
    return lowerResolvedCall(name, expression.location, std::move(arguments));
  }
  case ExprKind::NamedArgument: {
    const auto& named = static_cast<const NamedArgumentExpr&>(expression);
    diagnostics_.error(expression.location,
                       "named argument is valid only inside a call",
                       DiagnosticCode::Arity);
    return lowerExpression(*named.value, expected);
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
  case ExprKind::Await: {
    const auto& await = static_cast<const AwaitExpr&>(expression);
    auto task = lowerExpression(*await.value);
    if (!currentAsync_)
      diagnostics_.error(expression.location,
                         "await is valid only inside an async function",
                         DiagnosticCode::AwaitContext);
    if (currentAsync_) {
      std::unordered_set<SymbolId> checked;
      for (const auto& scope : scopes_)
        for (const auto& [unused, symbol] : scope)
          if (checked.insert(symbol).second && isNativeType(hir_.symbol(symbol).type))
            diagnostics_.error(
                expression.location,
                "native value '" + hir_.symbol(symbol).name +
                    "' cannot remain live across await",
                DiagnosticCode::AsyncSuspension);
    }
    if (!isTaskType(task->type) || task->type.arguments.size() != 1) {
      if (task->type != Type::Invalid)
        diagnostics_.error(expression.location,
                           "await requires a Task[T] operand",
                           DiagnosticCode::AwaitContext);
      return std::make_unique<HirAwaitExpr>(expression.location, Type::Invalid,
                                            std::move(task));
    }
    Type outcome{TypeKind::Enum, "Result",
                 {task->type.arguments.front(), Type::String}};
    return std::make_unique<HirAwaitExpr>(expression.location, std::move(outcome),
                                          std::move(task));
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
    for (const auto& capture : captures) {
      if (isMoveOnlyType(hir_.symbol(capture.source).type))
        diagnostics_.error(
            lambda.location,
            "move-only value '" + capture.name +
                "' cannot be captured by a reusable closure",
            DiagnosticCode::MoveOnly);
      declaration.fields.push_back({capture.name, hir_.symbol(capture.source).type,
                                    lambda.location});
    }
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
    hir_.symbols[callable].parameterNames.push_back("$closure");
    for (const auto& parameter : lambda.parameters)
      hir_.symbols[callable].parameterNames.push_back(parameter.name);
    functions_.emplace(callableName, callable);
    pendingLambdas_.push_back({&lambda, callable, declarationIndex, closureType,
                               captures, currentSubstitutions_});
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

bool HirLowerer::isSendType(const Type& root) const {
  std::unordered_set<std::string> visiting;
  std::function<bool(const Type&, bool)> check = [&](const Type& type, bool sharing) {
    if (type == Type::Int || type == Type::Float || type == Type::Bool ||
        type == Type::Char || type == Type::String || type == Type::Unit)
      return true;
    if (isArrayType(type) || isSliceType(type))
      return type.arguments.size() == 1 && check(type.arguments[0], sharing);
    if (isUniqueBufferType(type))
      return !sharing && type.arguments.size() == 1 && check(type.arguments[0], true);
    if (isWeakType(type))
      return type.arguments.size() == 1 && check(type.arguments[0], true);
    if (isTaskType(type))
      return !sharing && type.arguments.size() == 1 && check(type.arguments[0], false);
    if (type.declaration == "std.thread.Thread")
      return !sharing && type.arguments.size() == 1 && check(type.arguments[0], false);
    if (type.declaration == "std.cancel.CancellationToken" ||
        type.declaration == "std.sync.Mutex" ||
        type.declaration == "std.sync.Event" ||
        type.declaration == "std.sync.AtomicInt" ||
        type.declaration == "std.sync.Once" ||
        type.declaration == "std.channel.Channel" ||
        type.declaration == "std.channel.Sender" ||
        type.declaration == "std.channel.Receiver")
      return true;
    if (type.declaration == "std.sync.LockGuard") return false;
    if (type.kind != TypeKind::Struct && type.kind != TypeKind::Enum) return false;
    if (type.declaration == "std.string.Builder") return false;
    const std::string key = typeName(type) + (sharing ? "#share" : "#send");
    if (!visiting.insert(key).second) return true;
    const std::uint32_t index = findTypeDeclaration(type);
    if (index == static_cast<std::uint32_t>(-1)) return false;
    const auto& declaration = hir_.typeDeclarations[index];
    Substitutions substitutions;
    for (std::size_t argument = 0;
         argument < declaration.typeParameters.size() && argument < type.arguments.size();
         ++argument)
      substitutions.emplace(declaration.typeParameters[argument], type.arguments[argument]);
    bool accepted = true;
    for (const auto& field : declaration.fields)
      accepted = accepted && check(substitute(field.type, substitutions), sharing);
    for (const auto& variant : declaration.variants)
      for (const auto& payload : variant.payloadTypes)
        accepted = accepted && check(substitute(payload, substitutions), sharing);
    visiting.erase(key);
    return accepted;
  };
  return check(root, false);
}

bool HirLowerer::isShareType(const Type& root) const {
  std::unordered_set<std::string> visiting;
  std::function<bool(const Type&)> check = [&](const Type& type) {
    if (type == Type::Int || type == Type::Float || type == Type::Bool ||
        type == Type::Char || type == Type::String || type == Type::Unit)
      return true;
    if (isArrayType(type) || isSliceType(type) || isWeakType(type))
      return type.arguments.size() == 1 && check(type.arguments[0]);
    if (isTaskType(type)) return false;
    if (type.declaration == "std.thread.Thread") return false;
    if (type.declaration == "std.cancel.CancellationToken" ||
        type.declaration == "std.sync.Mutex" ||
        type.declaration == "std.sync.Event" ||
        type.declaration == "std.sync.AtomicInt" ||
        type.declaration == "std.sync.Once" ||
        type.declaration == "std.channel.Channel" ||
        type.declaration == "std.channel.Sender" ||
        type.declaration == "std.channel.Receiver")
      return true;
    if (isUniqueBufferType(type) || type.declaration == "std.sync.LockGuard" ||
        (type.kind != TypeKind::Struct && type.kind != TypeKind::Enum))
      return false;
    if (type.declaration == "std.string.Builder") return false;
    const std::string key = typeName(type);
    if (!visiting.insert(key).second) return true;
    const std::uint32_t index = findTypeDeclaration(type);
    if (index == static_cast<std::uint32_t>(-1)) return false;
    const auto& declaration = hir_.typeDeclarations[index];
    Substitutions substitutions;
    for (std::size_t argument = 0;
         argument < declaration.typeParameters.size() && argument < type.arguments.size();
         ++argument)
      substitutions.emplace(declaration.typeParameters[argument], type.arguments[argument]);
    bool accepted = true;
    for (const auto& field : declaration.fields)
      accepted = accepted && check(substitute(field.type, substitutions));
    for (const auto& variant : declaration.variants)
      for (const auto& payload : variant.payloadTypes)
        accepted = accepted && check(substitute(payload, substitutions));
    visiting.erase(key);
    return accepted;
  };
  return check(root);
}

bool HirLowerer::isMoveOnlyType(const Type& root) const {
  std::unordered_set<std::string> visiting;
  std::function<bool(const Type&)> check = [&](const Type& type) {
    if (type == Type::Invalid) return false;
    if (isUniqueBufferType(type) || isTaskType(type) ||
        type.declaration == "std.sync.LockGuard" ||
        type.declaration == "std.task.TaskGroup" ||
        type.declaration == "std.thread.Thread")
      return true;
    if (isArrayType(type) || isSliceType(type) ||
        ((type.declaration == "Option" || type.declaration == "Result") &&
         !type.arguments.empty())) {
      for (const Type& argument : type.arguments)
        if (check(argument)) return true;
      return false;
    }
    if (type.kind != TypeKind::Struct && type.kind != TypeKind::Enum)
      return false;
    const std::string key = typeName(type);
    if (!visiting.insert(key).second) return false;
    const std::uint32_t declarationIndex = findTypeDeclaration(type);
    if (declarationIndex == static_cast<std::uint32_t>(-1)) {
      visiting.erase(key);
      return false;
    }
    const auto& declaration = hir_.typeDeclarations[declarationIndex];
    Substitutions substitutions;
    for (std::size_t index = 0;
         index < declaration.typeParameters.size() && index < type.arguments.size();
         ++index)
      substitutions.emplace(declaration.typeParameters[index], type.arguments[index]);
    bool moveOnly = false;
    for (const auto& field : declaration.fields)
      moveOnly = moveOnly || check(substitute(field.type, substitutions));
    for (const auto& variant : declaration.variants)
      for (const Type& payload : variant.payloadTypes)
        moveOnly = moveOnly || check(substitute(payload, substitutions));
    visiting.erase(key);
    return moveOnly;
  };
  return check(root);
}

void HirLowerer::diagnoseAsyncArguments(
    const std::vector<std::unique_ptr<HirExpr>>& arguments,
    const Location& location) const {
  for (const auto& argument : arguments)
    if (argument->type != Type::Invalid && !isSendType(argument->type))
      diagnostics_.error(
          argument->location.file.empty() ? location : argument->location,
          "async task argument of type " + typeName(argument->type) +
              " does not satisfy Send",
          DiagnosticCode::SendConstraint);
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
    if (statement->kind == HirStmtKind::Unsafe &&
        definitelyReturns(static_cast<const HirUnsafeStmt&>(*statement).body))
      return true;
  }
  return false;
}

} // namespace rocket
