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

enum class MirRvalueKind { Use, Unary, Binary, Call };

struct MirRvalue {
  MirRvalueKind kind = MirRvalueKind::Use;
  Type type = Type::Invalid;
  TokenKind op = TokenKind::End;
  MirOperand left;
  MirOperand right;
  SymbolId callee = InvalidSymbol;
  std::vector<MirOperand> arguments;

  static MirRvalue use(MirOperand operand);
  static MirRvalue unary(Type type, TokenKind op, MirOperand operand);
  static MirRvalue binary(Type type, TokenKind op, MirOperand left, MirOperand right);
  static MirRvalue call(Type type, SymbolId callee, std::vector<MirOperand> arguments);
};

struct MirInstruction {
  MirLocalId destination = InvalidMirLocal;
  MirRvalue value;
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
  std::vector<MirFunction> functions;
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
