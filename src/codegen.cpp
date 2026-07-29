#include "codegen.h"

#include <cctype>
#include <sstream>

namespace rocket {

std::string BootstrapCodeGenerator::cppType(Type type) {
  switch (type.kind) {
  case TypeKind::Int: return "std::int64_t";
  case TypeKind::Float: return "double";
  case TypeKind::Bool: return "bool";
  case TypeKind::Char: return "char";
  case TypeKind::String: return "std::string";
  case TypeKind::Unit: return "RocketUnit";
  case TypeKind::Array:
    return "RocketArray<" + cppType(collectionElementType(type)) + ">";
  case TypeKind::Slice:
    return "RocketSlice<" + cppType(collectionElementType(type)) + ">";
  case TypeKind::Struct:
    if (type.declaration == "std.string.Builder") return "RocketStringBuilder";
    return "RocketAggregate";
  case TypeKind::Enum: return "RocketAggregate";
  case TypeKind::TypeParameter:
  case TypeKind::Invalid: break;
  }
  return "void";
}

std::string BootstrapCodeGenerator::functionName(SymbolId symbol) const {
  std::string name = module_.symbols.at(symbol).name;
  for (char& character : name)
    if (!std::isalnum(static_cast<unsigned char>(character)) && character != '_')
      character = '_';
  return "rocket_fn_" + name + "_" + std::to_string(symbol);
}

std::string BootstrapCodeGenerator::localName(MirLocalId local) {
  return "rocket_l_" + std::to_string(local);
}

std::string BootstrapCodeGenerator::escaped(const std::string& text) {
  std::string result;
  for (char c : text) {
    switch (c) {
    case '\\': result += "\\\\"; break;
    case '"': result += "\\\""; break;
    case '\n': result += "\\n"; break;
    case '\r': result += "\\r"; break;
    case '\t': result += "\\t"; break;
    default: result.push_back(c); break;
    }
  }
  return result;
}

std::string BootstrapCodeGenerator::escapedCharacter(const std::string& text) {
  if (text == "\\") return "\\\\";
  if (text == "'") return "\\'";
  if (text == "\n") return "\\n";
  if (text == "\r") return "\\r";
  if (text == "\t") return "\\t";
  return text;
}

const char* BootstrapCodeGenerator::standardFunctionName(Intrinsic intrinsic) {
  switch (intrinsic) {
  case Intrinsic::StringByteLength: return "rocket_std_string_byte_length";
  case Intrinsic::StringConcat: return "rocket_std_string_concat";
  case Intrinsic::StringContains: return "rocket_std_string_contains";
  case Intrinsic::StringStartsWith: return "rocket_std_string_starts_with";
  case Intrinsic::StringEndsWith: return "rocket_std_string_ends_with";
  case Intrinsic::StringTrim: return "rocket_std_string_trim";
  case Intrinsic::StringSplit: return "rocket_std_string_split";
  case Intrinsic::StringByteAt: return "rocket_std_string_byte_at";
  case Intrinsic::StringByteValueAt: return "rocket_std_string_byte_value_at";
  case Intrinsic::StringSlice: return "rocket_std_string_slice";
  case Intrinsic::StringParseInt: return "rocket_std_string_parse_int";
  case Intrinsic::StringFromInt: return "rocket_std_string_from_int";
  case Intrinsic::StringBuilderNew: return "rocket_std_string_builder";
  case Intrinsic::StringBuilderAppend: return "rocket_std_string_builder_append";
  case Intrinsic::StringBuilderFinish: return "rocket_std_string_builder_finish";
  case Intrinsic::CollectionsLength: return "rocket_std_collections_length";
  case Intrinsic::CollectionsReverse: return "rocket_std_collections_reverse";
  case Intrinsic::CollectionsConcat: return "rocket_std_collections_concat";
  case Intrinsic::CollectionsJoin: return "rocket_std_collections_join";
  case Intrinsic::FileReadText: return "rocket_std_file_read_text";
  case Intrinsic::FileWriteText: return "rocket_std_file_write_text";
  case Intrinsic::FileAppendText: return "rocket_std_file_append_text";
  case Intrinsic::FileExists: return "rocket_std_file_exists";
  case Intrinsic::FileRemove: return "rocket_std_file_remove";
  case Intrinsic::FileList: return "rocket_std_file_list";
  case Intrinsic::FileCreateDirectory: return "rocket_std_file_create_directory";
  case Intrinsic::PathJoin: return "rocket_std_path_join";
  case Intrinsic::PathBasename: return "rocket_std_path_basename";
  case Intrinsic::PathExtension: return "rocket_std_path_extension";
  case Intrinsic::PathNormalize: return "rocket_std_path_normalize";
  case Intrinsic::JsonParse: return "rocket_std_json_parse";
  case Intrinsic::JsonStringify: return "rocket_std_json_stringify";
  case Intrinsic::CsvParse: return "rocket_std_csv_parse";
  case Intrinsic::CsvEncode: return "rocket_std_csv_encode";
  case Intrinsic::RandomSeed: return "rocket_std_random_seed";
  case Intrinsic::RandomInt: return "rocket_std_random_int";
  case Intrinsic::RandomFloat: return "rocket_std_random_float";
  case Intrinsic::ProcessRun: return "rocket_std_process_run";
  case Intrinsic::ProcessArguments: return "rocket_std_process_arguments";
  case Intrinsic::ProcessExecutablePath: return "rocket_std_process_executable_path";
  case Intrinsic::ProcessEnvironment: return "rocket_std_process_environment";
  case Intrinsic::ProcessWorkingDirectory: return "rocket_std_process_working_directory";
  case Intrinsic::TimeUnixMilliseconds: return "rocket_std_time_unix_milliseconds";
  case Intrinsic::TimeMonotonicMilliseconds: return "rocket_std_time_monotonic_milliseconds";
  case Intrinsic::TimeSleepMilliseconds: return "rocket_std_time_sleep_milliseconds";
  default: return nullptr;
  }
}

std::string BootstrapCodeGenerator::generate() const {
  std::ostringstream out;
  out << "// Generated by rocketc bootstrap backend from verified MIR. Do not edit.\n"
         "#include <any>\n#include <cstdint>\n#include <cstdlib>\n#include <initializer_list>\n"
         "#include <iostream>\n#include <limits>\n#include <memory>\n#include <string>\n"
         "#include <utility>\n#include <vector>\n\n"
         "struct RocketUnit {};\n"
         "constexpr bool operator==(RocketUnit, RocketUnit) { return true; }\n"
         "template <typename T> RocketUnit rocket_print(const T& value) { "
         "std::cout << value << '\\n'; return {}; }\n"
         "template <typename T> using RocketArray = std::shared_ptr<std::vector<T>>;\n"
         "template <typename T> struct RocketSlice { RocketArray<T> owner; "
         "std::int64_t offset{}; std::int64_t length{}; };\n"
         "struct RocketAggregateData { std::uint32_t tag{}; std::vector<std::any> fields; };\n"
         "using RocketAggregate = std::shared_ptr<RocketAggregateData>;\n"
         "inline RocketAggregate rocket_aggregate(std::uint32_t tag, std::vector<std::any> fields) { "
         "return std::make_shared<RocketAggregateData>(RocketAggregateData{tag, std::move(fields)}); }\n"
         "template <typename T> T rocket_field(const RocketAggregate& value, std::size_t index) { "
         "return std::any_cast<T>(value->fields.at(index)); }\n"
         "inline std::int64_t rocket_tag(const RocketAggregate& value) { return value->tag; }\n"
         "[[noreturn]] inline void rocket_bounds_error() { "
         "std::cerr << \"rocket runtime error: collection bounds failure\\n\"; std::exit(101); }\n"
         "[[noreturn]] inline void rocket_integer_error(const char* message) { "
         "std::cerr << \"rocket runtime error: \" << message << '\\n'; std::exit(101); }\n"
         "inline std::int64_t rocket_int_add(std::int64_t left, std::int64_t right) { "
         "if ((right > 0 && left > std::numeric_limits<std::int64_t>::max() - right) || "
         "(right < 0 && left < std::numeric_limits<std::int64_t>::min() - right)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left + right; }\n"
         "inline std::int64_t rocket_int_sub(std::int64_t left, std::int64_t right) { "
         "if ((right < 0 && left > std::numeric_limits<std::int64_t>::max() + right) || "
         "(right > 0 && left < std::numeric_limits<std::int64_t>::min() + right)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left - right; }\n"
         "inline std::int64_t rocket_int_mul(std::int64_t left, std::int64_t right) { "
         "if (left == 0 || right == 0) return 0; "
         "if ((left == -1 && right == std::numeric_limits<std::int64_t>::min()) || "
         "(right == -1 && left == std::numeric_limits<std::int64_t>::min())) "
         "rocket_integer_error(\"Int arithmetic overflow\"); "
         "if ((left > 0 && right > 0 && left > std::numeric_limits<std::int64_t>::max() / right) || "
         "(left > 0 && right < 0 && right < std::numeric_limits<std::int64_t>::min() / left) || "
         "(left < 0 && right > 0 && left < std::numeric_limits<std::int64_t>::min() / right) || "
         "(left < 0 && right < 0 && right < std::numeric_limits<std::int64_t>::max() / left)) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left * right; }\n"
         "inline std::int64_t rocket_int_div(std::int64_t left, std::int64_t right) { "
         "if (right == 0) rocket_integer_error(\"Int division by zero\"); "
         "if (left == std::numeric_limits<std::int64_t>::min() && right == -1) "
         "rocket_integer_error(\"Int arithmetic overflow\"); return left / right; }\n"
         "template <typename T> RocketArray<T> rocket_array(std::initializer_list<T> values) { "
         "return std::make_shared<std::vector<T>>(values); }\n"
         "template <typename T> RocketArray<T> rocket_array_update(const RocketArray<T>& values, "
         "std::int64_t index, T value) { "
         "if (index < 0 || index >= static_cast<std::int64_t>(values->size())) rocket_bounds_error(); "
         "RocketArray<T> result = values.use_count() == 1 ? values : std::make_shared<std::vector<T>>(*values); "
         "(*result)[static_cast<std::size_t>(index)] = std::move(value); return result; }\n"
         "template <typename T> T rocket_index(const RocketArray<T>& values, std::int64_t index) { "
         "if (index < 0 || index >= static_cast<std::int64_t>(values->size())) rocket_bounds_error(); "
         "return (*values)[static_cast<std::size_t>(index)]; }\n"
         "template <typename T> T rocket_index(const RocketSlice<T>& values, std::int64_t index) { "
         "if (index < 0 || index >= values.length) rocket_bounds_error(); "
         "return (*values.owner)[static_cast<std::size_t>(values.offset + index)]; }\n"
         "template <typename T> RocketSlice<T> rocket_slice(const RocketArray<T>& values, "
         "std::int64_t start, std::int64_t end) { "
         "if (start < 0 || end < start || end > static_cast<std::int64_t>(values->size())) "
         "rocket_bounds_error(); return {values, start, end - start}; }\n"
         "template <typename T> RocketSlice<T> rocket_slice(const RocketSlice<T>& values, "
         "std::int64_t start, std::int64_t end) { "
         "if (start < 0 || end < start || end > values.length) rocket_bounds_error(); "
         "return {values.owner, values.offset + start, end - start}; }\n"
         "#include \"stage0_stdlib.h\"\n\n";
  for (const auto& function : module_.functions) {
    out << cppType(function.result) << ' ' << functionName(function.symbol) << '(';
    for (std::size_t i = 0; i < function.parameters.size(); ++i) {
      if (i) out << ", ";
      const MirLocalId parameter = function.parameters[i];
      out << cppType(function.locals[parameter].type) << ' ' << localName(parameter);
    }
    out << ");\n";
  }
  out << '\n';
  for (const auto& function : module_.functions) emitFunction(out, function);

  for (const auto& function : module_.functions) {
    if (module_.symbols[function.symbol].name == "main") {
      out << "int main(int argc, char** argv) { rocket_std_process_set_arguments(argc, argv); "
          << "return static_cast<int>(" << functionName(function.symbol) << "()); }\n";
      break;
    }
  }
  return out.str();
}

void BootstrapCodeGenerator::emitFunction(std::ostream& out,
                                          const MirFunction& function) const {
  out << cppType(function.result) << ' ' << functionName(function.symbol) << '(';
  for (std::size_t i = 0; i < function.parameters.size(); ++i) {
    if (i) out << ", ";
    const MirLocalId parameter = function.parameters[i];
    out << cppType(function.locals[parameter].type) << ' ' << localName(parameter);
  }
  out << ") {\n";
  for (MirLocalId local = 0; local < function.locals.size(); ++local) {
    if (!function.locals[local].parameter)
      out << "    " << cppType(function.locals[local].type) << ' ' << localName(local) << "{};\n";
  }
  out << "    goto rocket_bb_0;\n";
  for (std::size_t block = 0; block < function.blocks.size(); ++block) {
    out << "rocket_bb_" << block << ":\n";
    for (const auto& instruction : function.blocks[block].instructions)
      emitInstruction(out, instruction);
    emitTerminator(out, *function.blocks[block].terminator, function.result);
  }
  out << "}\n\n";
}

void BootstrapCodeGenerator::emitInstruction(std::ostream& out,
                                             const MirInstruction& instruction) const {
  if (instruction.kind != MirInstructionKind::Assign) {
    out << "    // bootstrap RAII: "
        << (instruction.kind == MirInstructionKind::Retain ? "retain " : "release ");
    emitOperand(out, instruction.arcOperand);
    out << "\n";
    return;
  }
  out << "    " << localName(instruction.destination) << " = ";
  emitRvalue(out, instruction.value);
  out << ";\n";
}

void BootstrapCodeGenerator::emitTerminator(std::ostream& out,
                                            const MirTerminator& terminator,
                                            Type functionResult) const {
  switch (terminator.kind) {
  case MirTerminatorKind::Goto:
    out << "    goto rocket_bb_" << terminator.target << ";\n";
    break;
  case MirTerminatorKind::Branch:
    out << "    if (";
    emitOperand(out, terminator.condition);
    out << ") goto rocket_bb_" << terminator.thenTarget << "; else goto rocket_bb_"
        << terminator.elseTarget << ";\n";
    break;
  case MirTerminatorKind::Return:
    out << "    return";
    if (terminator.returned.has_value()) {
      out << ' ';
      emitOperand(out, *terminator.returned);
    } else if (functionResult == Type::Unit) {
      out << " {}";
    }
    out << ";\n";
    break;
  }
}

void BootstrapCodeGenerator::emitRvalue(std::ostream& out, const MirRvalue& value) const {
  switch (value.kind) {
  case MirRvalueKind::Use:
    emitOperand(out, value.left);
    break;
  case MirRvalueKind::Unary:
    if (value.op == TokenKind::KwNot) {
      out << "(!";
      emitOperand(out, value.left);
      out << ')';
    } else if (value.type == Type::Int) {
      out << "rocket_int_sub(0, ";
      emitOperand(out, value.left);
      out << ')';
    } else {
      out << "(-";
      emitOperand(out, value.left);
      out << ')';
    }
    break;
  case MirRvalueKind::Binary: {
    if (value.left.type == Type::Int &&
        (value.op == TokenKind::Plus || value.op == TokenKind::Minus ||
         value.op == TokenKind::Star || value.op == TokenKind::Slash)) {
      const char* helper = value.op == TokenKind::Plus ? "rocket_int_add" :
                           value.op == TokenKind::Minus ? "rocket_int_sub" :
                           value.op == TokenKind::Star ? "rocket_int_mul" : "rocket_int_div";
      out << helper << '(';
      emitOperand(out, value.left);
      out << ", ";
      emitOperand(out, value.right);
      out << ')';
      break;
    }
    const char* op = value.op == TokenKind::KwAnd ? "&&" :
                     value.op == TokenKind::KwOr ? "||" : tokenName(value.op);
    out << '(';
    emitOperand(out, value.left);
    out << ' ' << op << ' ';
    emitOperand(out, value.right);
    out << ')';
    break;
  }
  case MirRvalueKind::Call:
    if (module_.symbols[value.callee].kind == SymbolKind::BuiltinFunction) {
      const HirSymbol& symbol = module_.symbols[value.callee];
      if (symbol.intrinsic == Intrinsic::Print) out << "rocket_print";
      else if (const char* name = standardFunctionName(symbol.intrinsic)) out << name;
      else out << "/* unknown standard-library intrinsic */";
    } else {
      out << functionName(value.callee);
    }
    out << '(';
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << ')';
    break;
  case MirRvalueKind::Array:
    out << "rocket_array<" << cppType(collectionElementType(value.type)) << ">({";
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << "})";
    break;
  case MirRvalueKind::ArrayUpdate:
    out << "rocket_array_update(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ", ";
    emitOperand(out, value.end);
    out << ')';
    break;
  case MirRvalueKind::Index:
    out << "rocket_index(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ')';
    break;
  case MirRvalueKind::Slice:
    out << "rocket_slice(";
    emitOperand(out, value.left);
    out << ", ";
    emitOperand(out, value.right);
    out << ", ";
    emitOperand(out, value.end);
    out << ')';
    break;
  case MirRvalueKind::Aggregate:
    out << "rocket_aggregate(" << value.tag << ", std::vector<std::any>{";
    for (std::size_t i = 0; i < value.arguments.size(); ++i) {
      if (i) out << ", ";
      emitOperand(out, value.arguments[i]);
    }
    out << "})";
    break;
  case MirRvalueKind::Field:
    out << "rocket_field<" << cppType(value.type) << ">(";
    emitOperand(out, value.left);
    out << ", " << value.tag << ')';
    break;
  case MirRvalueKind::Tag:
    out << "rocket_tag(";
    emitOperand(out, value.left);
    out << ')';
    break;
  }
}

void BootstrapCodeGenerator::emitOperand(std::ostream& out,
                                         const MirOperand& operand) const {
  if (operand.kind == MirOperandKind::Local) {
    out << localName(operand.local);
    return;
  }
  switch (operand.type.kind) {
  case TypeKind::Int: out << operand.constant << "LL"; break;
  case TypeKind::Float: out << operand.constant; break;
  case TypeKind::Bool: out << operand.constant; break;
  case TypeKind::Char: out << '\'' << escapedCharacter(operand.constant) << '\''; break;
  case TypeKind::String: out << "std::string{\"" << escaped(operand.constant) << "\"}"; break;
  case TypeKind::Unit: out << "RocketUnit{}"; break;
  case TypeKind::Array:
  case TypeKind::Slice:
  case TypeKind::Struct:
  case TypeKind::Enum:
    out << "/* invalid aggregate constant */";
    break;
  case TypeKind::TypeParameter:
  case TypeKind::Invalid: out << "/* invalid */"; break;
  }
}

} // namespace rocket
