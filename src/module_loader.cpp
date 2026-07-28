#include "module_loader.h"

#include "lexer.h"
#include "parser.h"
#include "type.h"

#include <algorithm>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace rocket {
namespace {

bool readSource(const std::filesystem::path& path, std::string& source) {
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  source = buffer.str();
  return true;
}

std::string lastComponent(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::string qualified(const std::string& module, const std::string& name) {
  return module.empty() ? name : module + "." + name;
}

struct LoadedModule {
  std::string name;
  std::filesystem::path path;
  Module ast;
  std::unordered_set<std::string> functions;
  std::unordered_set<std::string> types;
  std::unordered_set<std::string> variants;
  std::unordered_set<std::string> publicFunctions;
  std::unordered_set<std::string> publicTypes;
  std::unordered_set<std::string> publicVariants;
  std::unordered_map<std::string, std::string> aliases;
};

class Loader {
public:
  Loader(std::filesystem::path root, std::filesystem::path packageRoot,
         Diagnostics& diagnostics)
      : rootPath_(std::filesystem::absolute(std::move(root)).lexically_normal()),
        packageRoot_(std::filesystem::absolute(std::move(packageRoot)).lexically_normal()),
        diagnostics_(diagnostics) {}

  std::optional<Module> load() {
    if (!loadOne("", rootPath_, {rootPath_.string(), 1, 1})) return std::nullopt;
    buildIndexes();
    for (auto& [name, module] : modules_) rewrite(module);
    if (diagnostics_.hasErrors()) return std::nullopt;

    Module merged;
    merged.name.clear();
    for (const auto& name : order_) {
      auto& module = modules_.at(name);
      for (auto& structure : module.ast.structs)
        merged.structs.push_back(std::move(structure));
      for (auto& enumeration : module.ast.enums)
        merged.enums.push_back(std::move(enumeration));
      for (auto& function : module.ast.functions)
        merged.functions.push_back(std::move(function));
    }
    return merged;
  }

private:
  bool loadOne(const std::string& name, const std::filesystem::path& path,
               const Location& importLocation) {
    const int state = states_[name];
    if (state == 2) return true;
    if (state == 1) {
      diagnostics_.error(importLocation,
                         "import cycle detected at module '" +
                             (name.empty() ? std::string("<root>") : name) + "'",
                         DiagnosticCode::ImportCycle);
      return false;
    }
    states_[name] = 1;
    std::string source;
    if (!readSource(path, source)) {
      diagnostics_.error(importLocation, "could not read imported module '" +
                                               (name.empty() ? path.string() : name) + "'",
                         DiagnosticCode::ModuleNotFound);
      states_[name] = 2;
      return false;
    }
    Lexer lexer(path.string(), std::move(source), diagnostics_);
    auto tokens = lexer.lex();
    Parser parser(tokens, diagnostics_);
    Module ast = parser.parseModule();
    ast.name = name;
    auto [inserted, unused] = modules_.emplace(name, LoadedModule{name, path, std::move(ast)});
    LoadedModule& module = inserted->second;

    bool valid = true;
    for (const auto& import : module.ast.imports) {
      if (import.name.rfind("std.", 0) == 0) continue;
      std::filesystem::path importedPath = packageRoot_;
      std::size_t start = 0;
      while (start < import.name.size()) {
        const std::size_t dot = import.name.find('.', start);
        importedPath /= import.name.substr(start, dot == std::string::npos
                                                      ? std::string::npos
                                                      : dot - start);
        if (dot == std::string::npos) break;
        start = dot + 1;
      }
      importedPath += ".rocket";
      valid = loadOne(import.name, importedPath.lexically_normal(), import.location) && valid;
    }
    states_[name] = 2;
    order_.push_back(name);
    return valid;
  }

  void buildIndexes() {
    for (auto& [name, module] : modules_) {
      for (const auto& function : module.ast.functions) {
        module.functions.insert(function.name);
        if (function.publicDeclaration) module.publicFunctions.insert(function.name);
      }
      for (const auto& structure : module.ast.structs) {
        module.types.insert(structure.name);
        if (structure.publicDeclaration) module.publicTypes.insert(structure.name);
      }
      for (const auto& enumeration : module.ast.enums) {
        module.types.insert(enumeration.name);
        if (enumeration.publicDeclaration) module.publicTypes.insert(enumeration.name);
        for (const auto& variant : enumeration.variants) {
          module.variants.insert(variant.name);
          if (enumeration.publicDeclaration) module.publicVariants.insert(variant.name);
        }
      }
      for (const auto& import : module.ast.imports) {
        const std::string alias = lastComponent(import.name);
        if (auto found = module.aliases.find(alias);
            found != module.aliases.end() && found->second != import.name) {
          diagnostics_.error(import.location, "import alias '" + alias + "' is ambiguous",
                             DiagnosticCode::ImportAlias);
        } else {
          module.aliases.emplace(alias, import.name);
          module.aliases.emplace(import.name, import.name);
        }
      }
    }
  }

  std::optional<std::string> externalMember(LoadedModule& module,
                                             const std::string& spelling,
                                             const Location& location,
                                             const char* category) {
    const std::size_t dot = spelling.rfind('.');
    if (dot == std::string::npos) return std::nullopt;
    const std::string prefix = spelling.substr(0, dot);
    const std::string member = spelling.substr(dot + 1);
    auto alias = module.aliases.find(prefix);
    if (alias == module.aliases.end()) return std::nullopt;
    if (alias->second.rfind("std.", 0) == 0)
      return qualified(alias->second, member);
    auto imported = modules_.find(alias->second);
    if (imported == modules_.end()) return std::nullopt;
    const auto& target = imported->second;
    const bool visible = std::string(category) == "function"
                             ? target.publicFunctions.contains(member)
                         : std::string(category) == "type"
                             ? target.publicTypes.contains(member)
                             : target.publicVariants.contains(member);
    if (!visible)
      diagnostics_.error(location, "module '" + alias->second + "' has no public " +
                                       category + " '" + member + "'",
                         DiagnosticCode::Visibility);
    return qualified(alias->second, member);
  }

  Type rewriteTypeNode(LoadedModule& module, Type type,
                       const std::unordered_set<std::string>& typeParameters,
                       const Location& location) {
    for (auto& argument : type.arguments)
      argument = rewriteTypeNode(module, std::move(argument), typeParameters, location);
    if (type.kind != TypeKind::Struct) return type;
    if (typeParameters.contains(type.declaration) || type.declaration == "Option" ||
        type.declaration == "Result")
      return type;
    if (module.types.contains(type.declaration)) {
      type.declaration = qualified(module.name, type.declaration);
    } else if (auto external = externalMember(module, type.declaration, location, "type")) {
      type.declaration = *external;
    }
    return type;
  }

  void rewriteTypeSpelling(LoadedModule& module, std::string& spelling,
                           const std::unordered_set<std::string>& typeParameters,
                           const Location& location) {
    Type parsed = typeFromName(spelling);
    if (parsed == Type::Invalid) return;
    spelling = typeName(rewriteTypeNode(module, std::move(parsed), typeParameters, location));
  }

  std::optional<std::string> flattenedName(const Expr& expression) {
    if (expression.kind == ExprKind::Name)
      return static_cast<const LiteralExpr&>(expression).value;
    if (expression.kind != ExprKind::Field) return std::nullopt;
    const auto& field = static_cast<const FieldExpr&>(expression);
    auto prefix = flattenedName(*field.value);
    return prefix ? std::optional<std::string>(*prefix + "." + field.field) : std::nullopt;
  }

  std::string rewriteCallableName(LoadedModule& module, const std::string& spelling,
                                  const Location& location) {
    if (spelling == "print" || spelling == "Some" || spelling == "None" ||
        spelling == "Ok" || spelling == "Err")
      return spelling;
    if (module.functions.contains(spelling) || module.types.contains(spelling) ||
        module.variants.contains(spelling))
      return qualified(module.name, spelling);
    if (spelling.find('.') != std::string::npos) {
      const std::size_t dot = spelling.rfind('.');
      const std::string prefix = spelling.substr(0, dot);
      const std::string member = spelling.substr(dot + 1);
      auto alias = module.aliases.find(prefix);
      if (alias != module.aliases.end()) {
        if (alias->second.rfind("std.", 0) == 0)
          return qualified(alias->second, member);
        const auto& imported = modules_.at(alias->second);
        if (imported.publicFunctions.contains(member) ||
            imported.publicTypes.contains(member) ||
            imported.publicVariants.contains(member))
          return qualified(alias->second, member);
        diagnostics_.error(location, "module '" + alias->second +
                                         "' has no public callable '" + member + "'",
                           DiagnosticCode::Visibility);
      }
    }
    return spelling;
  }

  void rewriteExpression(LoadedModule& module, std::unique_ptr<Expr>& expression) {
    switch (expression->kind) {
    case ExprKind::Unary:
      rewriteExpression(module, static_cast<UnaryExpr&>(*expression).operand); break;
    case ExprKind::Binary: {
      auto& binary = static_cast<BinaryExpr&>(*expression);
      rewriteExpression(module, binary.left);
      rewriteExpression(module, binary.right);
      break;
    }
    case ExprKind::Call: {
      auto& call = static_cast<CallExpr&>(*expression);
      for (auto& argument : call.arguments) rewriteExpression(module, argument);
      if (auto name = flattenedName(*call.callee)) {
        const std::string rewritten = rewriteCallableName(module, *name, call.callee->location);
        call.callee = std::make_unique<LiteralExpr>(ExprKind::Name, call.callee->location,
                                                    rewritten);
      } else {
        rewriteExpression(module, call.callee);
      }
      break;
    }
    case ExprKind::Array:
      for (auto& element : static_cast<ArrayExpr&>(*expression).elements)
        rewriteExpression(module, element);
      break;
    case ExprKind::Index: {
      auto& index = static_cast<IndexExpr&>(*expression);
      rewriteExpression(module, index.collection);
      rewriteExpression(module, index.index);
      break;
    }
    case ExprKind::Slice: {
      auto& slice = static_cast<SliceExpr&>(*expression);
      rewriteExpression(module, slice.collection);
      rewriteExpression(module, slice.start);
      rewriteExpression(module, slice.end);
      break;
    }
    case ExprKind::Field:
      rewriteExpression(module, static_cast<FieldExpr&>(*expression).value); break;
    case ExprKind::Propagate:
      rewriteExpression(module, static_cast<PropagateExpr&>(*expression).value); break;
    default: break;
    }
  }

  void rewriteBlock(LoadedModule& module,
                    std::vector<std::unique_ptr<Stmt>>& statements,
                    const std::unordered_set<std::string>& typeParameters) {
    for (auto& statement : statements) {
      switch (statement->kind) {
      case StmtKind::Binding: {
        auto& binding = static_cast<BindingStmt&>(*statement);
        if (!binding.declaredType.empty())
          rewriteTypeSpelling(module, binding.declaredType, typeParameters, binding.location);
        rewriteExpression(module, binding.initializer);
        break;
      }
      case StmtKind::Assignment:
        rewriteExpression(module, static_cast<AssignmentStmt&>(*statement).value); break;
      case StmtKind::Return: {
        auto& returned = static_cast<ReturnStmt&>(*statement);
        if (returned.value) rewriteExpression(module, returned.value);
        break;
      }
      case StmtKind::Expression:
        rewriteExpression(module, static_cast<ExprStmt&>(*statement).expression); break;
      case StmtKind::If: {
        auto& branch = static_cast<IfStmt&>(*statement);
        rewriteExpression(module, branch.condition);
        rewriteBlock(module, branch.thenBody, typeParameters);
        rewriteBlock(module, branch.elseBody, typeParameters);
        break;
      }
      case StmtKind::While: {
        auto& loop = static_cast<WhileStmt&>(*statement);
        rewriteExpression(module, loop.condition);
        rewriteBlock(module, loop.body, typeParameters);
        break;
      }
      case StmtKind::For: {
        auto& loop = static_cast<ForStmt&>(*statement);
        rewriteExpression(module, loop.start);
        rewriteExpression(module, loop.end);
        rewriteBlock(module, loop.body, typeParameters);
        break;
      }
      case StmtKind::Match: {
        auto& match = static_cast<MatchStmt&>(*statement);
        rewriteExpression(module, match.value);
        for (auto& matchCase : match.cases) {
          if (!matchCase.pattern.wildcard && matchCase.pattern.variant != "Some" &&
              matchCase.pattern.variant != "None" && matchCase.pattern.variant != "Ok" &&
              matchCase.pattern.variant != "Err") {
            if (module.variants.contains(matchCase.pattern.variant))
              matchCase.pattern.variant = qualified(module.name, matchCase.pattern.variant);
            else if (auto external = externalMember(module, matchCase.pattern.variant,
                                                     matchCase.pattern.location, "variant"))
              matchCase.pattern.variant = *external;
          }
          rewriteBlock(module, matchCase.body, typeParameters);
        }
        break;
      }
      case StmtKind::Break:
      case StmtKind::Continue: break;
      }
    }
  }

  void rewrite(LoadedModule& module) {
    for (auto& structure : module.ast.structs) {
      const std::unordered_set<std::string> parameters(structure.typeParameters.begin(),
                                                        structure.typeParameters.end());
      structure.name = qualified(module.name, structure.name);
      for (auto& field : structure.fields)
        rewriteTypeSpelling(module, field.typeName, parameters, field.location);
    }
    for (auto& enumeration : module.ast.enums) {
      const std::unordered_set<std::string> parameters(enumeration.typeParameters.begin(),
                                                        enumeration.typeParameters.end());
      enumeration.name = qualified(module.name, enumeration.name);
      for (auto& variant : enumeration.variants) {
        variant.name = qualified(module.name, variant.name);
        for (auto& payload : variant.payloadTypes)
          rewriteTypeSpelling(module, payload, parameters, variant.location);
      }
    }
    for (auto& function : module.ast.functions) {
      const std::unordered_set<std::string> parameters(function.typeParameters.begin(),
                                                        function.typeParameters.end());
      function.name = qualified(module.name, function.name);
      for (auto& parameter : function.parameters)
        rewriteTypeSpelling(module, parameter.typeName, parameters, parameter.location);
      rewriteTypeSpelling(module, function.returnType, parameters, function.location);
      rewriteBlock(module, function.body, parameters);
    }
  }

  std::filesystem::path rootPath_;
  std::filesystem::path packageRoot_;
  Diagnostics& diagnostics_;
  std::map<std::string, LoadedModule> modules_;
  std::unordered_map<std::string, int> states_;
  std::vector<std::string> order_;
};

} // namespace

std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      Diagnostics& diagnostics) {
  return Loader(rootPath, std::filesystem::absolute(rootPath).parent_path(), diagnostics).load();
}

std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      const std::filesystem::path& packageRoot,
                                      Diagnostics& diagnostics) {
  return Loader(rootPath, packageRoot, diagnostics).load();
}

} // namespace rocket
