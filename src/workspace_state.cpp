#include "workspace_state.h"

#include "lexer.h"
#include "parser.h"

#include <functional>
#include <sstream>
#include <string_view>

namespace rocket {
namespace {
void appendText(std::ostringstream &result, std::string_view value) {
  result << value.size() << ':' << value;
}

void appendParameters(std::ostringstream &result,
                      const std::vector<Parameter> &parameters) {
  result << parameters.size() << ';';
  for (const auto &parameter : parameters) {
    appendText(result, parameter.name);
    appendText(result, parameter.typeName);
    appendText(result, parameter.defaultText);
  }
}

std::string exportedInterface(const Module &module) {
  std::ostringstream result;
  result << module.imports.size() << ';';
  for (const auto &item : module.imports)
    appendText(result, item.name);
  for (const auto &item : module.functions) {
    if (!item.publicDeclaration)
      continue;
    appendText(result, "fn");
    appendText(result, item.name);
    appendText(result, item.returnType);
    appendText(result, item.methodOwner);
    appendText(result, item.methodTrait);
    appendText(result, item.nativeName);
    result << item.associatedConstant << item.nativeImport << item.nativeExport
           << item.nativeConstant << item.asynchronous << ';';
    result << item.typeParameters.size() << ';';
    for (const auto &type : item.typeParameters)
      appendText(result, type);
    appendParameters(result, item.parameters);
    result << item.constraints.size() << ';';
    for (const auto &constraint : item.constraints) {
      appendText(result, constraint.typeParameter);
      appendText(result, constraint.traitName);
    }
  }
  for (const auto &item : module.structs) {
    if (!item.publicDeclaration)
      continue;
    appendText(result, "struct");
    appendText(result, item.name);
    result << static_cast<int>(item.representation) << ';';
    result << item.typeParameters.size() << ';';
    for (const auto &type : item.typeParameters)
      appendText(result, type);
    result << item.fields.size() << ';';
    for (const auto &field : item.fields) {
      appendText(result, field.name);
      appendText(result, field.typeName);
    }
    appendParameters(result, item.callbackParameters);
    appendText(result, item.callbackReturnType);
    appendText(result, item.nativeName);
  }
  for (const auto &item : module.enums) {
    if (!item.publicDeclaration)
      continue;
    appendText(result, "enum");
    appendText(result, item.name);
    result << item.typeParameters.size() << ';';
    for (const auto &type : item.typeParameters)
      appendText(result, type);
    result << item.variants.size() << ';';
    for (const auto &variant : item.variants) {
      appendText(result, variant.name);
      result << variant.payloadNames.size() << ';';
      for (const auto &name : variant.payloadNames)
        appendText(result, name);
      result << variant.payloadTypes.size() << ';';
      for (const auto &type : variant.payloadTypes)
        appendText(result, type);
    }
  }
  for (const auto &item : module.traits) {
    if (!item.publicDeclaration)
      continue;
    appendText(result, "trait");
    appendText(result, item.name);
    result << item.methods.size() << ';';
    for (const auto &method : item.methods) {
      appendText(result, method.name);
      appendParameters(result, method.parameters);
      appendText(result, method.returnType);
    }
  }
  return result.str();
}
} // namespace

void WorkspaceState::erase(const std::filesystem::path &path) {
  parsed_.erase(std::filesystem::absolute(path).lexically_normal());
}

void WorkspaceState::retain(const std::set<std::filesystem::path> &sources) {
  for (auto it = parsed_.begin(); it != parsed_.end();) {
    if (!sources.contains(it->first))
      it = parsed_.erase(it);
    else
      ++it;
  }
}

const ParsedSource *
WorkspaceState::find(const std::filesystem::path &path) const {
  const auto found =
      parsed_.find(std::filesystem::absolute(path).lexically_normal());
  return found == parsed_.end() ? nullptr : found->second.get();
}

const ParsedSource &WorkspaceState::parse(const std::filesystem::path &path,
                                          const std::string &source) {
  const auto key = std::filesystem::absolute(path).lexically_normal();
  const std::size_t hash = std::hash<std::string>{}(source);
  const auto found = parsed_.find(key);
  if (found != parsed_.end() && found->second->hash == hash &&
      found->second->source == source)
    return *found->second;
  auto parsed = std::make_shared<ParsedSource>();
  parsed->identity = key;
  parsed->source = source;
  parsed->hash = hash;
  parsed->tokens = Lexer(key.string(), source, parsed->diagnostics).lex();
  if (!parsed->diagnostics.hasErrors())
    parsed->module = Parser(parsed->tokens, parsed->diagnostics).parseModule();
  parsed->structure =
      buildDocumentStructure(parsed->module, parsed->tokens, source);
  parsed->exportedInterface = exportedInterface(parsed->module);
  parsed->interfaceHash = std::hash<std::string>{}(parsed->exportedInterface);
  return *(parsed_[key] = std::move(parsed));
}

} // namespace rocket
