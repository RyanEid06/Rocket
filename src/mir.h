#pragma once

#include "hir.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace rocket {

using MirLocalId = std::uint32_t;
using MirBlockId = std::uint32_t;
inline constexpr MirLocalId InvalidMirLocal = static_cast<MirLocalId>(-1);
inline constexpr MirBlockId InvalidMirBlock = static_cast<MirBlockId>(-1);

enum class MirOperandKind { Constant, Local };

struct MirOperand {
  MirOperandKind kind = MirOperandKind::Constant;
  Type type = Type::Invalid;
  std::string constant;
  MirLocalId local = InvalidMirLocal;

  static MirOperand constantValue(Type type, std::string value);
  static MirOperand localValue(Type type, MirLocalId local);
};

enum class MirRvalueKind {
  Use, Unary, Binary, Call, Array, ArrayUpdate, Index, Slice, Aggregate, Field, Tag
};

struct MirRvalue {
  MirRvalueKind kind = MirRvalueKind::Use;
  Type type = Type::Invalid;
  TokenKind op = TokenKind::End;
  MirOperand left;
  MirOperand right;
  MirOperand end;
  SymbolId callee = InvalidSymbol;
  std::uint32_t declaration = 0;
  std::uint32_t tag = 0;
  std::vector<MirOperand> arguments;

  static MirRvalue use(MirOperand operand);
  static MirRvalue unary(Type type, TokenKind op, MirOperand operand);
  static MirRvalue binary(Type type, TokenKind op, MirOperand left, MirOperand right);
  static MirRvalue call(Type type, SymbolId callee, std::vector<MirOperand> arguments);
  static MirRvalue array(Type type, std::vector<MirOperand> elements);
  static MirRvalue arrayUpdate(Type type, MirOperand array, MirOperand index,
                               MirOperand value);
  static MirRvalue index(Type type, MirOperand collection, MirOperand index);
  static MirRvalue slice(Type type, MirOperand collection, MirOperand start, MirOperand end);
  static MirRvalue aggregate(Type type, std::uint32_t declaration, std::uint32_t tag,
                             std::vector<MirOperand> arguments);
  static MirRvalue field(Type type, MirOperand aggregate, std::uint32_t field);
  static MirRvalue tagOf(MirOperand aggregate);
};

enum class MirInstructionKind { Assign, Retain, Release };

struct MirInstruction {
  MirInstructionKind kind = MirInstructionKind::Assign;
  MirLocalId destination = InvalidMirLocal;
  MirRvalue value;
  MirOperand arcOperand;

  static MirInstruction assign(MirLocalId destination, MirRvalue value);
  static MirInstruction retain(MirOperand operand);
  static MirInstruction release(MirOperand operand);
};

enum class MirTerminatorKind { Goto, Branch, Return };

struct MirTerminator {
  MirTerminatorKind kind = MirTerminatorKind::Return;
  MirBlockId target = InvalidMirBlock;
  MirOperand condition;
  MirBlockId thenTarget = InvalidMirBlock;
  MirBlockId elseTarget = InvalidMirBlock;
  std::optional<MirOperand> returned;

  static MirTerminator goTo(MirBlockId target);
  static MirTerminator branch(MirOperand condition, MirBlockId thenTarget,
                              MirBlockId elseTarget);
  static MirTerminator returnValue(std::optional<MirOperand> returned);
};

struct MirBasicBlock {
  std::vector<MirInstruction> instructions;
  std::optional<MirTerminator> terminator;
};

struct MirLocal {
  Type type = Type::Invalid;
  SymbolId sourceSymbol = InvalidSymbol;
  bool parameter = false;
};

struct MirFunction {
  SymbolId symbol = InvalidSymbol;
  Type result = Type::Invalid;
  std::vector<MirLocal> locals;
  std::vector<MirLocalId> parameters;
  std::vector<MirBasicBlock> blocks;
};

struct MirModule {
  std::vector<HirSymbol> symbols;
  std::vector<HirTypeDeclaration> typeDeclarations;
  std::vector<MirFunction> functions;
  bool library = false;
};

class MirLowerer {
public:
  explicit MirLowerer(const HirModule& hir) : hir_(hir) {}
  MirModule lower();

private:
  struct LoopTargets { MirBlockId breakTarget; MirBlockId continueTarget; };

  MirFunction lowerFunction(const HirFunction& function);
  std::optional<MirBlockId> lowerBlock(const HirBlock& body, MirBlockId current);
  std::optional<MirBlockId> lowerStatement(const HirStmt& statement, MirBlockId current);
  MirOperand lowerExpression(const HirExpr& expression, MirBlockId& current);
  MirOperand lowerShortCircuit(const HirBinaryExpr& expression, MirBlockId& current);
  MirLocalId addLocal(Type type, SymbolId sourceSymbol = InvalidSymbol, bool parameter = false);
  MirLocalId localForSymbol(SymbolId symbol);
  MirLocalId addInstruction(MirBlockId block, MirRvalue value,
                            MirLocalId destination = InvalidMirLocal);
  void addRetain(MirBlockId block, MirOperand operand);
  void addRelease(MirBlockId block, MirOperand operand);
  void releaseOwnedLocals(MirBlockId block);
  MirOperand materialize(MirBlockId block, MirOperand operand);
  MirBlockId addBlock();
  void terminate(MirBlockId block, MirTerminator terminator);

  const HirModule& hir_;
  MirModule mir_;
  MirFunction* function_ = nullptr;
  std::vector<MirLocalId> symbolLocals_;
  std::vector<LoopTargets> loops_;
};

bool verifyMir(const MirModule& module, std::string& error);
std::string dumpMir(const MirModule& module);

} // namespace rocket
