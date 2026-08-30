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

std::filesystem::path standardLibraryRoot = ROCKETC_STDLIB_SOURCE_PATH;
constexpr std::uintmax_t MaximumSourceFileBytes = 4U * 1024U * 1024U;
constexpr std::size_t MaximumProjectSourceBytes = 64U * 1024U * 1024U;
constexpr std::size_t MaximumProjectModules = 4096U;
constexpr std::size_t MaximumImportDepth = 64U;

bool readSourceFile(const std::filesystem::path& path, std::string& source,
                    std::string& error) {
  std::error_code sizeError;
  const std::uintmax_t size = std::filesystem::file_size(path, sizeError);
  if (!sizeError && size > MaximumSourceFileBytes) {
    error = "source file exceeds the 4 MiB limit";
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  if (!input) return false;
  std::ostringstream buffer;
  buffer << input.rdbuf();
  source = buffer.str();
  if (source.size() > MaximumSourceFileBytes) {
    error = "source file exceeds the 4 MiB limit";
    source.clear();
    return false;
  }
  return true;
}

std::string lastComponent(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

std::string qualified(const std::string& module, const std::string& name) {
  return module.empty() ? name : module + "." + name;
}

std::string localFunctionName(const Function& function) {
  return function.name;
}

struct LoadedModule {
  std::string name;
  std::filesystem::path path;
  Module ast;
  std::unordered_set<std::string> functions;
  std::unordered_set<std::string> types;
  std::unordered_set<std::string> variants;
  std::unordered_set<std::string> traits;
  std::unordered_set<std::string> publicFunctions;
  std::unordered_set<std::string> publicTypes;
  std::unordered_set<std::string> publicVariants;
  std::unordered_set<std::string> publicTraits;
  std::unordered_map<std::string, std::string> aliases;
  std::unordered_map<std::string, std::string> importTargets;
};

class Loader {
public:
  Loader(std::filesystem::path root, std::filesystem::path packageRoot,
         std::filesystem::path targetSourceRoot,
         std::vector<PackageDependencyRoot> dependencyRoots,
         Diagnostics& diagnostics, const SourceOverlays* overlays = nullptr)
      : rootPath_(std::filesystem::absolute(std::move(root)).lexically_normal()),
        packageRoot_(std::filesystem::absolute(std::move(packageRoot)).lexically_normal()),
        targetSourceRoot_(targetSourceRoot.empty()
                              ? std::filesystem::path{}
                              : std::filesystem::absolute(
                                    std::move(targetSourceRoot)).lexically_normal()),
        diagnostics_(diagnostics), overlays_(overlays) {
    for (auto& dependency : dependencyRoots) {
      if (dependency.direct) rootDependencies_.insert(dependency.name);
      dependencyRoots_.emplace(dependency.name, std::move(dependency));
    }
  }

  std::optional<Module> load() {
    if (!loadOne("", rootPath_, {rootPath_.string(), 1, 1}, packageRoot_,
                 targetSourceRoot_, "", rootDependencies_, 0))
      return std::nullopt;
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
      for (auto& trait : module.ast.traits)
        merged.traits.push_back(std::move(trait));
      for (auto& function : module.ast.functions)
        merged.functions.push_back(std::move(function));
    }
    return merged;
  }

private:
  bool sourceExists(const std::filesystem::path& path) const {
    const auto normalized = std::filesystem::absolute(path).lexically_normal();
    return (overlays_ != nullptr && overlays_->contains(normalized)) ||
           std::filesystem::is_regular_file(normalized);
  }

  bool readSource(const std::filesystem::path& path, std::string& source,
                  std::string& error) const {
    const auto normalized = std::filesystem::absolute(path).lexically_normal();
    if (overlays_ != nullptr) {
      const auto found = overlays_->find(normalized);
      if (found != overlays_->end()) {
        if (found->second.size() > MaximumSourceFileBytes) {
          error = "source overlay exceeds the 4 MiB limit";
          return false;
        }
        source = found->second;
        return true;
      }
    }
    return readSourceFile(normalized, source, error);
  }

  bool loadOne(const std::string& name, const std::filesystem::path& path,
               const Location& importLocation,
               const std::filesystem::path& ownerRoot,
               const std::filesystem::path& ownerTargetRoot,
               const std::string& ownerPrefix,
               const std::unordered_set<std::string>& allowedDependencies,
               std::size_t depth) {
    const auto foundState = states_.find(name);
    const int state = foundState == states_.end() ? 0 : foundState->second;
    if (state == 2) return true;
    if (state == 1) {
      diagnostics_.error(importLocation,
                         "import cycle detected at module '" +
                             (name.empty() ? std::string("<root>") : name) + "'",
                         DiagnosticCode::ImportCycle);
      return false;
    }
    if (depth >= MaximumImportDepth) {
      diagnostics_.error(importLocation,
                         "import nesting exceeds the 64-level limit",
                         DiagnosticCode::ResourceLimit);
      return false;
    }
    if (states_.size() >= MaximumProjectModules) {
      diagnostics_.error(importLocation,
                         "module graph exceeds the 4096-file limit",
                         DiagnosticCode::ResourceLimit);
      return false;
    }
    states_[name] = 1;
    std::string source;
    std::string readError;
    if (!readSource(path, source, readError)) {
      diagnostics_.error(
          importLocation,
          readError.empty()
              ? "could not read imported module '" +
                    (name.empty() ? path.string() : name) + "'"
              : readError + " in '" + path.string() + "'",
          readError.empty() ? DiagnosticCode::ModuleNotFound
                            : DiagnosticCode::ResourceLimit);
      states_[name] = 2;
      return false;
    }
    if (sourceBytes_ > MaximumProjectSourceBytes - source.size()) {
      diagnostics_.error(importLocation,
                         "module graph exceeds the 64 MiB source limit",
                         DiagnosticCode::ResourceLimit);
      states_[name] = 2;
      return false;
    }
    sourceBytes_ += source.size();
    Lexer lexer(path.string(), std::move(source), diagnostics_);
    auto tokens = lexer.lex();
    Parser parser(tokens, diagnostics_);
    Module ast = parser.parseModule();
    ast.name = name;
    auto [inserted, unused] = modules_.emplace(name, LoadedModule{name, path, std::move(ast)});
    LoadedModule& module = inserted->second;

    bool valid = true;
    for (const auto& import : module.ast.imports) {
      if (import.name == "std.testing") {
        module.importTargets[import.name] = import.name;
        valid = loadOne(import.name,
                        (standardLibraryRoot / "std/testing.rocket").lexically_normal(),
                        import.location, standardLibraryRoot, {}, "std", {},
                        depth + 1) && valid;
        continue;
      }
      if (import.name.rfind("std.", 0) == 0) continue;
      const std::size_t firstDot = import.name.find('.');
      const std::string first = import.name.substr(0, firstDot);
      std::filesystem::path importedPath;
      std::string importedName;
      std::filesystem::path importedOwnerRoot = ownerRoot;
      std::filesystem::path importedOwnerTargetRoot = ownerTargetRoot;
      std::string importedOwnerPrefix = ownerPrefix;
      std::unordered_set<std::string> importedAllowed = allowedDependencies;
      if (allowedDependencies.contains(first)) {
        const auto dependency = dependencyRoots_.find(first);
        if (dependency == dependencyRoots_.end()) {
          diagnostics_.error(import.location,
                             "locked dependency root is unavailable for '" + first + "'",
                             DiagnosticCode::DependencyImport);
          valid = false;
          continue;
        }
        importedName = import.name;
        importedOwnerRoot = dependency->second.root;
        importedOwnerTargetRoot = dependency->second.targetSourceRoot;
        importedOwnerPrefix = dependency->second.name;
        importedAllowed = std::unordered_set<std::string>(
            dependency->second.dependencies.begin(),
            dependency->second.dependencies.end());
        if (firstDot == std::string::npos) {
          importedPath = dependency->second.entry;
        } else {
          std::filesystem::path relative;
          std::size_t start = firstDot + 1;
          while (start < import.name.size()) {
            const std::size_t dot = import.name.find('.', start);
            relative /= import.name.substr(
                start, dot == std::string::npos ? std::string::npos : dot - start);
            if (dot == std::string::npos) break;
            start = dot + 1;
          }
          relative += ".rocket";
          importedPath = dependency->second.root / relative;
          if (!dependency->second.targetSourceRoot.empty()) {
            const auto candidate =
                dependency->second.targetSourceRoot / relative;
            if (sourceExists(candidate)) importedPath = candidate;
          }
        }
      } else if (dependencyRoots_.contains(first)) {
        diagnostics_.error(
            import.location,
            "dependency import '" + import.name +
                "' is not a declared edge of the current locked package",
            DiagnosticCode::DependencyImport);
        valid = false;
        continue;
      } else {
        std::filesystem::path relative;
        std::size_t start = 0;
        while (start < import.name.size()) {
          const std::size_t dot = import.name.find('.', start);
          relative /= import.name.substr(start, dot == std::string::npos
                                                    ? std::string::npos
                                                    : dot - start);
          if (dot == std::string::npos) break;
          start = dot + 1;
        }
        relative += ".rocket";
        importedPath = ownerRoot / relative;
        if (!ownerTargetRoot.empty()) {
          const auto candidate = ownerTargetRoot / relative;
          if (sourceExists(candidate)) importedPath = candidate;
        }
        importedName = ownerPrefix.empty() ? import.name
                                            : ownerPrefix + "." + import.name;
      }
      module.importTargets[import.name] = importedName;
      valid = loadOne(importedName, importedPath.lexically_normal(), import.location,
                      importedOwnerRoot, importedOwnerTargetRoot,
                      importedOwnerPrefix,
                      importedAllowed, depth + 1) && valid;
    }
    states_[name] = 2;
    order_.push_back(name);
    return valid;
  }

  void buildIndexes() {
    for (auto& [name, module] : modules_) {
      for (const auto& function : module.ast.functions) {
        const std::string callable = localFunctionName(function);
        module.functions.insert(callable);
        if (function.publicDeclaration) module.publicFunctions.insert(callable);
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
      for (const auto& trait : module.ast.traits) {
        module.traits.insert(trait.name);
        if (trait.publicDeclaration) module.publicTraits.insert(trait.name);
      }
      for (const auto& import : module.ast.imports) {
        const std::string alias = lastComponent(import.name);
        const auto target = module.importTargets.find(import.name);
        const std::string resolved = target == module.importTargets.end()
                                         ? import.name
                                         : target->second;
        if (auto found = module.aliases.find(alias);
            found != module.aliases.end() && found->second != resolved) {
          diagnostics_.error(import.location, "import alias '" + alias + "' is ambiguous",
                             DiagnosticCode::ImportAlias);
        } else {
          module.aliases.emplace(alias, resolved);
          module.aliases.emplace(import.name, resolved);
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
                         : std::string(category) == "trait"
                             ? target.publicTraits.contains(member)
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

  std::optional<std::string> rewriteCallableName(LoadedModule& module,
                                                 const std::string& spelling,
                                                 const Location& location) {
    if (spelling == "print" || spelling == "Some" || spelling == "None" ||
        spelling == "Ok" || spelling == "Err")
      return spelling;
    if (spelling == "String.from_int" || spelling == "String.builder")
      return spelling;
    if (module.functions.contains(spelling) || module.types.contains(spelling) ||
        module.variants.contains(spelling))
      return qualified(module.name, spelling);
    if (spelling.find('.') != std::string::npos) {
      // Resolve the longest imported-module prefix. The remaining suffix may
      // be a Phase 12 associated function such as Type.make.
      std::size_t dot = spelling.size();
      while ((dot = spelling.rfind('.', dot - 1)) != std::string::npos) {
        const std::string prefix = spelling.substr(0, dot);
        const std::string member = spelling.substr(dot + 1);
        auto alias = module.aliases.find(prefix);
        if (alias == module.aliases.end()) {
          if (dot == 0) break;
          continue;
        }
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
        return spelling;
      }
      // A dotted expression that is neither a declaration nor an import is an
      // instance method call and must retain its receiver for HIR resolution.
      return std::nullopt;
    }
    return spelling;
  }

  void rewriteExpression(LoadedModule& module, std::unique_ptr<Expr>& expression,
                         const std::unordered_set<std::string>& typeParameters) {
    switch (expression->kind) {
    case ExprKind::Await:
      rewriteExpression(module, static_cast<AwaitExpr&>(*expression).value,
                        typeParameters); break;
    case ExprKind::Unary:
      rewriteExpression(module, static_cast<UnaryExpr&>(*expression).operand,
                        typeParameters); break;
    case ExprKind::Binary: {
      auto& binary = static_cast<BinaryExpr&>(*expression);
      rewriteExpression(module, binary.left, typeParameters);
      rewriteExpression(module, binary.right, typeParameters);
      break;
    }
    case ExprKind::Call: {
      auto& call = static_cast<CallExpr&>(*expression);
      for (auto& argument : call.arguments)
        rewriteExpression(module, argument, typeParameters);
      if (auto name = flattenedName(*call.callee)) {
        if (auto rewritten = rewriteCallableName(module, *name, call.callee->location)) {
          call.callee = std::make_unique<LiteralExpr>(ExprKind::Name, call.callee->location,
                                                      *rewritten);
        } else {
          rewriteExpression(module, call.callee, typeParameters);
        }
      } else {
        rewriteExpression(module, call.callee, typeParameters);
      }
      break;
    }
    case ExprKind::NamedArgument:
      rewriteExpression(module,
                        static_cast<NamedArgumentExpr&>(*expression).value,
                        typeParameters);
      break;
    case ExprKind::Array:
      for (auto& element : static_cast<ArrayExpr&>(*expression).elements)
        rewriteExpression(module, element, typeParameters);
      break;
    case ExprKind::Index: {
      auto& index = static_cast<IndexExpr&>(*expression);
      rewriteExpression(module, index.collection, typeParameters);
      rewriteExpression(module, index.index, typeParameters);
      break;
    }
    case ExprKind::Slice: {
      auto& slice = static_cast<SliceExpr&>(*expression);
      rewriteExpression(module, slice.collection, typeParameters);
      rewriteExpression(module, slice.start, typeParameters);
      rewriteExpression(module, slice.end, typeParameters);
      break;
    }
    case ExprKind::Field: {
      if (auto name = flattenedName(*expression)) {
        if (auto rewritten = externalMember(module, *name, expression->location,
                                            "function")) {
          expression = std::make_unique<LiteralExpr>(ExprKind::Name,
                                                      expression->location,
                                                      *rewritten);
          break;
        }
      }
      rewriteExpression(module, static_cast<FieldExpr&>(*expression).value,
                        typeParameters);
      break;
    }
    case ExprKind::Propagate:
      rewriteExpression(module, static_cast<PropagateExpr&>(*expression).value,
                        typeParameters); break;
    case ExprKind::Lambda: {
      auto& lambda = static_cast<LambdaExpr&>(*expression);
      for (auto& parameter : lambda.parameters)
        rewriteTypeSpelling(module, parameter.typeName, typeParameters,
                            parameter.location);
      rewriteTypeSpelling(module, lambda.returnType, typeParameters, lambda.location);
      rewriteExpression(module, lambda.body, typeParameters);
      break;
    }
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
        rewriteExpression(module, binding.initializer, typeParameters);
        break;
      }
      case StmtKind::Assignment:
        rewriteExpression(module, static_cast<AssignmentStmt&>(*statement).value,
                          typeParameters); break;
      case StmtKind::IndexAssignment: {
        auto& assignment = static_cast<IndexAssignmentStmt&>(*statement);
        rewriteExpression(module, assignment.index, typeParameters);
        rewriteExpression(module, assignment.value, typeParameters);
        break;
      }
      case StmtKind::Return: {
        auto& returned = static_cast<ReturnStmt&>(*statement);
        if (returned.value) rewriteExpression(module, returned.value, typeParameters);
        break;
      }
      case StmtKind::Expression:
        rewriteExpression(module, static_cast<ExprStmt&>(*statement).expression,
                          typeParameters); break;
      case StmtKind::If: {
        auto& branch = static_cast<IfStmt&>(*statement);
        rewriteExpression(module, branch.condition, typeParameters);
        rewriteBlock(module, branch.thenBody, typeParameters);
        rewriteBlock(module, branch.elseBody, typeParameters);
        break;
      }
      case StmtKind::While: {
        auto& loop = static_cast<WhileStmt&>(*statement);
        rewriteExpression(module, loop.condition, typeParameters);
        rewriteBlock(module, loop.body, typeParameters);
        break;
      }
      case StmtKind::For: {
        auto& loop = static_cast<ForStmt&>(*statement);
        rewriteExpression(module, loop.start, typeParameters);
        if (loop.end) rewriteExpression(module, loop.end, typeParameters);
        rewriteBlock(module, loop.body, typeParameters);
        break;
      }
      case StmtKind::Match: {
        auto& match = static_cast<MatchStmt&>(*statement);
        rewriteExpression(module, match.value, typeParameters);
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
      case StmtKind::Unsafe:
        rewriteBlock(module, static_cast<UnsafeStmt&>(*statement).body,
                     typeParameters);
        break;
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
      for (auto& parameter : structure.callbackParameters)
        rewriteTypeSpelling(module, parameter.typeName, parameters, parameter.location);
      if (!structure.callbackReturnType.empty())
        rewriteTypeSpelling(module, structure.callbackReturnType, parameters,
                            structure.location);
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
    for (auto& trait : module.ast.traits) {
      trait.name = qualified(module.name, trait.name);
      const std::unordered_set<std::string> self{"Self"};
      for (auto& method : trait.methods) {
        for (auto& parameter : method.parameters)
          rewriteTypeSpelling(module, parameter.typeName, self, parameter.location);
        rewriteTypeSpelling(module, method.returnType, self, method.location);
      }
    }
    for (auto& function : module.ast.functions) {
      const std::unordered_set<std::string> parameters(function.typeParameters.begin(),
                                                        function.typeParameters.end());
      if (function.methodOwner.empty()) {
        function.name = qualified(module.name, function.name);
      } else {
        const std::size_t memberSeparator = function.name.rfind('.');
        const std::string member = memberSeparator == std::string::npos
                                       ? function.name
                                       : function.name.substr(memberSeparator + 1);
        rewriteTypeSpelling(module, function.methodOwner, parameters, function.location);
        if (!function.methodTrait.empty()) {
          if (module.traits.contains(function.methodTrait)) {
            function.methodTrait = qualified(module.name, function.methodTrait);
          } else if (auto external = externalMember(module, function.methodTrait,
                                                    function.location, "trait")) {
            function.methodTrait = *external;
          }
          function.name = function.methodOwner.substr(0, function.methodOwner.find('[')) +
                          "." + function.methodTrait + "." + member;
        } else {
          function.name = function.methodOwner.substr(0, function.methodOwner.find('[')) +
                          "." + member;
        }
      }
      for (auto& constraint : function.constraints) {
        if (module.traits.contains(constraint.traitName)) {
          constraint.traitName = qualified(module.name, constraint.traitName);
        } else if (auto external = externalMember(module, constraint.traitName,
                                                  constraint.location, "trait")) {
          constraint.traitName = *external;
        }
      }
      for (auto& parameter : function.parameters)
        rewriteTypeSpelling(module, parameter.typeName, parameters, parameter.location);
      rewriteTypeSpelling(module, function.returnType, parameters, function.location);
      rewriteBlock(module, function.body, parameters);
    }
  }

  std::filesystem::path rootPath_;
  std::filesystem::path packageRoot_;
  std::filesystem::path targetSourceRoot_;
  Diagnostics& diagnostics_;
  const SourceOverlays* overlays_ = nullptr;
  std::map<std::string, PackageDependencyRoot> dependencyRoots_;
  std::unordered_set<std::string> rootDependencies_;
  std::map<std::string, LoadedModule> modules_;
  std::unordered_map<std::string, int> states_;
  std::vector<std::string> order_;
  std::size_t sourceBytes_ = 0;
};

} // namespace

void setStandardLibraryRoot(std::filesystem::path root) {
  standardLibraryRoot = std::filesystem::absolute(std::move(root)).lexically_normal();
}

std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      Diagnostics& diagnostics) {
  return Loader(rootPath, std::filesystem::absolute(rootPath).parent_path(), {},
                {}, diagnostics).load();
}

std::optional<Module> loadModuleGraph(const std::filesystem::path& rootPath,
                                      const std::filesystem::path& packageRoot,
                                      Diagnostics& diagnostics) {
  return Loader(rootPath, packageRoot, {}, {}, diagnostics).load();
}

std::optional<Module> loadModuleGraph(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& packageRoot,
    const std::vector<PackageDependencyRoot>& dependencyRoots,
    Diagnostics& diagnostics) {
  return Loader(rootPath, packageRoot, {}, dependencyRoots, diagnostics).load();
}

std::optional<Module> loadModuleGraph(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& packageRoot,
    const std::filesystem::path& targetSourceRoot,
    const std::vector<PackageDependencyRoot>& dependencyRoots,
    Diagnostics& diagnostics) {
  return Loader(rootPath, packageRoot, targetSourceRoot, dependencyRoots,
                diagnostics).load();
}

std::optional<Module> loadModuleGraph(
    const std::filesystem::path& rootPath,
    const std::filesystem::path& packageRoot,
    const std::vector<PackageDependencyRoot>& dependencyRoots,
    const SourceOverlays& overlays, Diagnostics& diagnostics) {
  return Loader(rootPath, packageRoot, {}, dependencyRoots, diagnostics,
                &overlays).load();
}

} // namespace rocket
