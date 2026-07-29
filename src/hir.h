#pragma once

#include "ast.h"
#include "diagnostic.h"
#include "type.h"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace rocket {

using SymbolId = std::uint32_t;
inline constexpr SymbolId InvalidSymbol = static_cast<SymbolId>(-1);

enum class SymbolKind {
  BuiltinFunction, Function, Parameter, Local, LoopVariable, PatternBinding
};

enum class Intrinsic {
  None,
  Print,
  StringByteLength,
  StringConcat,
  StringContains,
  StringStartsWith,
  StringEndsWith,
  StringTrim,
  StringSplit,
  StringByteAt,
  StringByteValueAt,
  StringSlice,
  StringParseInt,
  StringFromInt,
  StringBuilderNew,
  StringBuilderAppend,
  StringBuilderFinish,
  CollectionsLength,
  CollectionsCapacity,
  CollectionsReserve,
  CollectionsAppend,
  CollectionsPop,
  CollectionsInsert,
  CollectionsRemove,
  CollectionsClear,
  CollectionsMapFromArrays,
  CollectionsMapLength,
  CollectionsMapFind,
  CollectionsMapGet,
  CollectionsMapKeys,
  CollectionsMapValues,
  CollectionsSetFromArray,
  CollectionsSetContains,
  CollectionsSetValues,
  CollectionsHash,
  CollectionsContains,
  CollectionsFind,
  CollectionsFilterEqual,
  CollectionsSortInt,
  CollectionsSortFloat,
  CollectionsSortChar,
  CollectionsSortString,
  CollectionsMapHash,
  CollectionsFoldSumInt,
  CollectionsFoldSumFloat,
  CollectionsReverse,
  CollectionsConcat,
  CollectionsJoin,
  FileReadText,
  FileWriteText,
  FileAppendText,
  FileExists,
  FileRemove,
  FileList,
  FileCreateDirectory,
  PathJoin,
  PathBasename,
  PathExtension,
  PathNormalize,
  JsonParse,
  JsonStringify,
  CsvParse,
  CsvEncode,
  RandomSeed,
  RandomInt,
  RandomFloat,
  ProcessRun,
  ProcessArguments,
  ProcessExecutablePath,
  ProcessEnvironment,
  ProcessWorkingDirectory,
  TimeUnixMilliseconds,
  TimeMonotonicMilliseconds,
  TimeSleepMilliseconds,
};

struct HirSymbol {
  SymbolId id = InvalidSymbol;
  SymbolKind kind = SymbolKind::Local;
  std::string name;
  Type type = Type::Invalid;
  bool mutableBinding = false;
  Location location;
  std::vector<Type> parameterTypes;
  Intrinsic intrinsic = Intrinsic::None;
};

enum class HirExprKind {
  Literal, Name, Unary, Binary, Call, Array, Index, Slice, Aggregate, Field, Propagate
};

struct HirExpr {
  HirExpr(HirExprKind kind, Location location, Type type)
      : kind(kind), location(std::move(location)), type(type) {}
  virtual ~HirExpr() = default;
  HirExprKind kind;
  Location location;
  Type type;
};

struct HirLiteralExpr final : HirExpr {
  HirLiteralExpr(Location location, Type type, std::string value)
      : HirExpr(HirExprKind::Literal, std::move(location), type), value(std::move(value)) {}
  std::string value;
};

struct HirNameExpr final : HirExpr {
  HirNameExpr(Location location, Type type, SymbolId symbol)
      : HirExpr(HirExprKind::Name, std::move(location), type), symbol(symbol) {}
  SymbolId symbol;
};

struct HirUnaryExpr final : HirExpr {
  HirUnaryExpr(Location location, Type type, TokenKind op, std::unique_ptr<HirExpr> operand)
      : HirExpr(HirExprKind::Unary, std::move(location), type), op(op),
        operand(std::move(operand)) {}
  TokenKind op;
  std::unique_ptr<HirExpr> operand;
};

struct HirBinaryExpr final : HirExpr {
  HirBinaryExpr(Location location, Type type, std::unique_ptr<HirExpr> left, TokenKind op,
                std::unique_ptr<HirExpr> right)
      : HirExpr(HirExprKind::Binary, std::move(location), type), left(std::move(left)),
        op(op), right(std::move(right)) {}
  std::unique_ptr<HirExpr> left;
  TokenKind op;
  std::unique_ptr<HirExpr> right;
};

struct HirCallExpr final : HirExpr {
  HirCallExpr(Location location, Type type, SymbolId callee,
              std::vector<std::unique_ptr<HirExpr>> arguments)
      : HirExpr(HirExprKind::Call, std::move(location), type), callee(callee),
        arguments(std::move(arguments)) {}
  SymbolId callee;
  std::vector<std::unique_ptr<HirExpr>> arguments;
};

struct HirArrayExpr final : HirExpr {
  HirArrayExpr(Location location, Type type, std::vector<std::unique_ptr<HirExpr>> elements)
      : HirExpr(HirExprKind::Array, std::move(location), type),
        elements(std::move(elements)) {}
  std::vector<std::unique_ptr<HirExpr>> elements;
};

struct HirIndexExpr final : HirExpr {
  HirIndexExpr(Location location, Type type, std::unique_ptr<HirExpr> collection,
               std::unique_ptr<HirExpr> index)
      : HirExpr(HirExprKind::Index, std::move(location), type),
        collection(std::move(collection)), index(std::move(index)) {}
  std::unique_ptr<HirExpr> collection;
  std::unique_ptr<HirExpr> index;
};

struct HirSliceExpr final : HirExpr {
  HirSliceExpr(Location location, Type type, std::unique_ptr<HirExpr> collection,
               std::unique_ptr<HirExpr> start, std::unique_ptr<HirExpr> end)
      : HirExpr(HirExprKind::Slice, std::move(location), type),
        collection(std::move(collection)), start(std::move(start)), end(std::move(end)) {}
  std::unique_ptr<HirExpr> collection;
  std::unique_ptr<HirExpr> start;
  std::unique_ptr<HirExpr> end;
};

struct HirAggregateExpr final : HirExpr {
  HirAggregateExpr(Location location, Type type, std::uint32_t declaration,
                   std::uint32_t tag, std::vector<std::unique_ptr<HirExpr>> arguments)
      : HirExpr(HirExprKind::Aggregate, std::move(location), std::move(type)),
        declaration(declaration), tag(tag), arguments(std::move(arguments)) {}
  std::uint32_t declaration = 0;
  std::uint32_t tag = 0;
  std::vector<std::unique_ptr<HirExpr>> arguments;
};

struct HirFieldExpr final : HirExpr {
  HirFieldExpr(Location location, Type type, std::unique_ptr<HirExpr> value,
               std::uint32_t field)
      : HirExpr(HirExprKind::Field, std::move(location), std::move(type)),
        value(std::move(value)), field(field) {}
  std::unique_ptr<HirExpr> value;
  std::uint32_t field = 0;
};

struct HirPropagateExpr final : HirExpr {
  HirPropagateExpr(Location location, Type type, std::unique_ptr<HirExpr> value,
                   Type functionResult, std::uint32_t declaration)
      : HirExpr(HirExprKind::Propagate, std::move(location), std::move(type)),
        value(std::move(value)), functionResult(std::move(functionResult)),
        declaration(declaration) {}
  std::unique_ptr<HirExpr> value;
  Type functionResult;
  std::uint32_t declaration = 0;
};

enum class HirStmtKind {
  Binding, Assignment, IndexAssignment, Return, Expression, If, While, For, Break,
  Continue, Match, ForEach
};

struct HirStmt {
  HirStmt(HirStmtKind kind, Location location) : kind(kind), location(std::move(location)) {}
  virtual ~HirStmt() = default;
  HirStmtKind kind;
  Location location;
};

using HirBlock = std::vector<std::unique_ptr<HirStmt>>;

struct HirBindingStmt final : HirStmt {
  HirBindingStmt(Location location, SymbolId symbol, std::unique_ptr<HirExpr> initializer)
      : HirStmt(HirStmtKind::Binding, std::move(location)), symbol(symbol),
        initializer(std::move(initializer)) {}
  SymbolId symbol;
  std::unique_ptr<HirExpr> initializer;
};

struct HirAssignmentStmt final : HirStmt {
  HirAssignmentStmt(Location location, SymbolId target, std::unique_ptr<HirExpr> value)
      : HirStmt(HirStmtKind::Assignment, std::move(location)), target(target),
        value(std::move(value)) {}
  SymbolId target;
  std::unique_ptr<HirExpr> value;
};

struct HirIndexAssignmentStmt final : HirStmt {
  HirIndexAssignmentStmt(Location location, SymbolId target,
                         std::unique_ptr<HirExpr> index,
                         std::unique_ptr<HirExpr> value)
      : HirStmt(HirStmtKind::IndexAssignment, std::move(location)), target(target),
        index(std::move(index)), value(std::move(value)) {}
  SymbolId target;
  std::unique_ptr<HirExpr> index;
  std::unique_ptr<HirExpr> value;
};

struct HirReturnStmt final : HirStmt {
  HirReturnStmt(Location location, std::unique_ptr<HirExpr> value)
      : HirStmt(HirStmtKind::Return, std::move(location)), value(std::move(value)) {}
  std::unique_ptr<HirExpr> value;
};

struct HirExprStmt final : HirStmt {
  HirExprStmt(Location location, std::unique_ptr<HirExpr> expression)
      : HirStmt(HirStmtKind::Expression, std::move(location)), expression(std::move(expression)) {}
  std::unique_ptr<HirExpr> expression;
};

struct HirIfStmt final : HirStmt {
  HirIfStmt(Location location, std::unique_ptr<HirExpr> condition, HirBlock thenBody,
            HirBlock elseBody)
      : HirStmt(HirStmtKind::If, std::move(location)), condition(std::move(condition)),
        thenBody(std::move(thenBody)), elseBody(std::move(elseBody)) {}
  std::unique_ptr<HirExpr> condition;
  HirBlock thenBody;
  HirBlock elseBody;
};

struct HirWhileStmt final : HirStmt {
  HirWhileStmt(Location location, std::unique_ptr<HirExpr> condition, HirBlock body)
      : HirStmt(HirStmtKind::While, std::move(location)), condition(std::move(condition)),
        body(std::move(body)) {}
  std::unique_ptr<HirExpr> condition;
  HirBlock body;
};

struct HirForStmt final : HirStmt {
  HirForStmt(Location location, SymbolId variable, std::unique_ptr<HirExpr> start,
             std::unique_ptr<HirExpr> end, HirBlock body)
      : HirStmt(HirStmtKind::For, std::move(location)), variable(variable),
        start(std::move(start)), end(std::move(end)), body(std::move(body)) {}
  SymbolId variable;
  std::unique_ptr<HirExpr> start;
  std::unique_ptr<HirExpr> end;
  HirBlock body;
};

struct HirLoopControlStmt final : HirStmt {
  HirLoopControlStmt(HirStmtKind kind, Location location) : HirStmt(kind, std::move(location)) {}
};

struct HirMatchCase {
  Location location;
  std::optional<std::uint32_t> tag;
  std::vector<SymbolId> bindings;
  HirBlock body;
};

struct HirMatchStmt final : HirStmt {
  HirMatchStmt(Location location, std::unique_ptr<HirExpr> value,
               std::uint32_t declaration, std::vector<HirMatchCase> cases)
      : HirStmt(HirStmtKind::Match, std::move(location)), value(std::move(value)),
        declaration(declaration), cases(std::move(cases)) {}
  std::unique_ptr<HirExpr> value;
  std::uint32_t declaration = 0;
  std::vector<HirMatchCase> cases;
};

struct HirParameter { SymbolId symbol = InvalidSymbol; };

struct HirFunction {
  SymbolId symbol = InvalidSymbol;
  Location location;
  std::vector<HirParameter> parameters;
  Type result = Type::Invalid;
  HirBlock body;
};

enum class HirTypeDeclKind { Struct, Enum };

struct HirField {
  std::string name;
  Type type;
  Location location;
};

struct HirVariant {
  std::string name;
  Location location;
  std::vector<Type> payloadTypes;
};

struct HirTypeDeclaration {
  HirTypeDeclKind kind = HirTypeDeclKind::Struct;
  std::string name;
  Location location;
  bool publicDeclaration = false;
  bool builtin = false;
  std::vector<std::string> typeParameters;
  std::vector<HirField> fields;
  std::vector<HirVariant> variants;
};

struct HirForEachStmt final : HirStmt {
  HirForEachStmt(Location location, SymbolId cursor, SymbolId variable,
                 std::unique_ptr<HirExpr> iterator,
                 std::unique_ptr<HirExpr> condition,
                 std::unique_ptr<HirExpr> value,
                 std::unique_ptr<HirExpr> advance, HirBlock body)
      : HirStmt(HirStmtKind::ForEach, std::move(location)), cursor(cursor),
        variable(variable), iterator(std::move(iterator)),
        condition(std::move(condition)), value(std::move(value)),
        advance(std::move(advance)), body(std::move(body)) {}
  SymbolId cursor;
  SymbolId variable;
  std::unique_ptr<HirExpr> iterator;
  std::unique_ptr<HirExpr> condition;
  std::unique_ptr<HirExpr> value;
  std::unique_ptr<HirExpr> advance;
  HirBlock body;
};

struct HirTraitMethod {
  std::string name;
  std::vector<Type> parameterTypes;
  Type result = Type::Invalid;
};

struct HirTraitDeclaration {
  std::string name;
  Location location;
  bool publicDeclaration = false;
  std::vector<HirTraitMethod> methods;
};

struct HirModule {
  std::vector<HirSymbol> symbols;
  std::vector<HirTypeDeclaration> typeDeclarations;
  std::vector<HirTraitDeclaration> traitDeclarations;
  std::vector<HirFunction> functions;

  const HirSymbol& symbol(SymbolId id) const { return symbols.at(id); }
};

class HirLowerer {
public:
  HirLowerer(const Module& module, Diagnostics& diagnostics)
      : ast_(module), diagnostics_(diagnostics) {}
  std::optional<HirModule> lower();

private:
  using Scope = std::unordered_map<std::string, SymbolId>;
  using Substitutions = std::unordered_map<std::string, Type>;

  struct VariantTarget { std::uint32_t declaration; std::uint32_t variant; };
  struct PendingSpecialization {
    const Function* function = nullptr;
    SymbolId symbol = InvalidSymbol;
    Substitutions substitutions;
    std::vector<Type> parameters;
    Type result = Type::Invalid;
  };
  struct StandardFunction {
    std::vector<std::string> typeParameters;
    std::vector<Type> parameterTypes;
    Type result = Type::Invalid;
    Intrinsic intrinsic = Intrinsic::None;
  };
  struct TraitImplementation {
    std::string traitName;
    Type ownerPattern = Type::Invalid;
    std::string member;
    const Function* function = nullptr;
  };
  struct LambdaCapture {
    std::string name;
    SymbolId source = InvalidSymbol;
    std::uint32_t field = 0;
  };
  struct PendingLambda {
    const LambdaExpr* lambda = nullptr;
    SymbolId symbol = InvalidSymbol;
    std::uint32_t declaration = 0;
    Type closureType = Type::Invalid;
    std::vector<LambdaCapture> captures;
  };
  struct ActiveCapture {
    SymbolId closure = InvalidSymbol;
    std::uint32_t field = 0;
    Type type = Type::Invalid;
  };

  SymbolId addSymbol(SymbolKind kind, const std::string& name, Type type, bool mutableBinding,
                     const Location& location, std::vector<Type> parameterTypes = {},
                     Intrinsic intrinsic = Intrinsic::None);
  HirFunction lowerFunction(const Function& function, SymbolId symbol);
  HirFunction lowerSpecialization(const PendingSpecialization& specialization);
  HirFunction lowerLambda(const PendingLambda& lambda);
  HirBlock lowerBlock(const std::vector<std::unique_ptr<Stmt>>& body, Type returnType, bool nested);
  std::unique_ptr<HirStmt> lowerStatement(const Stmt& statement, Type returnType);
  std::unique_ptr<HirExpr> lowerExpression(const Expr& expression,
                                           std::optional<Type> expected = std::nullopt);
  std::unique_ptr<HirExpr> lowerResolvedCall(
      const std::string& name, const Location& location,
      std::vector<std::unique_ptr<HirExpr>> arguments);
  Type resolveType(const std::string& spelling, const Location& location,
                   const Substitutions& substitutions = {});
  Type resolveParsedType(const Type& parsed, const Location& location,
                         const Substitutions& substitutions);
  Type substitute(const Type& pattern, const Substitutions& substitutions) const;
  bool inferTypeArguments(const Type& pattern, const Type& actual,
                          Substitutions& substitutions, const Location& location);
  std::uint32_t findTypeDeclaration(const Type& type) const;
  SymbolId specializeFunction(const Function& function,
                              const std::vector<std::unique_ptr<HirExpr>>& arguments,
                              const Location& location);
  void registerBuiltinTypes();
  void registerStandardLibrary();
  void registerTypeDeclarations();
  void registerTraits();
  bool typeImplementsTrait(const Type& type, const std::string& trait,
                           const Location& location, bool diagnoseAmbiguity = true) const;
  std::string traitMethodTarget(const Type& type, const std::string& member,
                                const Location& location) const;
  SymbolId findVariable(const std::string& name) const;
  bool definitelyReturns(const HirBlock& body) const;
  void collectLambdaCaptures(const Expr& expression,
                             const std::unordered_set<std::string>& parameters,
                             std::vector<LambdaCapture>& captures) const;

  const Module& ast_;
  Diagnostics& diagnostics_;
  HirModule hir_;
  std::unordered_map<std::string, SymbolId> functions_;
  std::unordered_map<std::string, const Function*> genericFunctions_;
  std::unordered_set<std::string> associatedConstants_;
  std::unordered_map<std::string, std::uint32_t> typeDeclarations_;
  std::unordered_map<std::string, std::uint32_t> traits_;
  std::vector<TraitImplementation> traitImplementations_;
  std::unordered_map<std::string, VariantTarget> variants_;
  std::unordered_map<std::string, StandardFunction> standardFunctions_;
  std::unordered_map<std::string, SymbolId> specializations_;
  std::vector<PendingSpecialization> pendingSpecializations_;
  std::vector<PendingLambda> pendingLambdas_;
  std::size_t userSpecializationCount_ = 0;
  std::vector<SymbolId> functionSymbols_;
  std::vector<Scope> scopes_;
  Substitutions currentSubstitutions_;
  std::unordered_map<std::string, ActiveCapture> activeCaptures_;
  Type currentReturnType_ = Type::Invalid;
  int loopDepth_ = 0;
};

} // namespace rocket
