#include "mir.h"

#include <sstream>
#include <utility>

namespace rocket {

MirOperand MirOperand::constantValue(Type type, std::string value) {
  MirOperand result;
  result.kind = MirOperandKind::Constant;
  result.type = type;
  result.constant = std::move(value);
  return result;
}

MirOperand MirOperand::localValue(Type type, MirLocalId local) {
  MirOperand result;
  result.kind = MirOperandKind::Local;
  result.type = type;
  result.local = local;
  return result;
}

MirRvalue MirRvalue::use(MirOperand operand) {
  MirRvalue result;
  result.kind = MirRvalueKind::Use;
  result.type = operand.type;
  result.left = std::move(operand);
  return result;
}

MirRvalue MirRvalue::unary(Type type, TokenKind op, MirOperand operand) {
  MirRvalue result;
  result.kind = MirRvalueKind::Unary;
  result.type = type;
  result.op = op;
  result.left = std::move(operand);
  return result;
}

MirRvalue MirRvalue::binary(Type type, TokenKind op, MirOperand left, MirOperand right) {
  MirRvalue result;
  result.kind = MirRvalueKind::Binary;
  result.type = type;
  result.op = op;
  result.left = std::move(left);
  result.right = std::move(right);
  return result;
}

MirRvalue MirRvalue::call(Type type, SymbolId callee, std::vector<MirOperand> arguments) {
  MirRvalue result;
  result.kind = MirRvalueKind::Call;
  result.type = type;
  result.callee = callee;
  result.arguments = std::move(arguments);
  return result;
}

MirTerminator MirTerminator::goTo(MirBlockId target) {
  MirTerminator result;
  result.kind = MirTerminatorKind::Goto;
  result.target = target;
  return result;
}

MirTerminator MirTerminator::branch(MirOperand condition, MirBlockId thenTarget,
                                    MirBlockId elseTarget) {
  MirTerminator result;
  result.kind = MirTerminatorKind::Branch;
  result.condition = std::move(condition);
  result.thenTarget = thenTarget;
  result.elseTarget = elseTarget;
  return result;
}

MirTerminator MirTerminator::returnValue(std::optional<MirOperand> returned) {
  MirTerminator result;
  result.kind = MirTerminatorKind::Return;
  result.returned = std::move(returned);
  return result;
}

MirModule MirLowerer::lower() {
  mir_ = {};
  mir_.symbols = hir_.symbols;
  for (const auto& function : hir_.functions)
    mir_.functions.push_back(lowerFunction(function));
  function_ = nullptr;
  return std::move(mir_);
}

MirFunction MirLowerer::lowerFunction(const HirFunction& function) {
  MirFunction result;
  result.symbol = function.symbol;
  result.result = function.result;
  function_ = &result;
  symbolLocals_.assign(hir_.symbols.size(), InvalidMirLocal);
  loops_.clear();

  for (const auto& parameter : function.parameters) {
    const Type type = hir_.symbol(parameter.symbol).type;
    const MirLocalId local = addLocal(type, parameter.symbol, true);
    symbolLocals_[parameter.symbol] = local;
    result.parameters.push_back(local);
  }

  const MirBlockId entry = addBlock();
  auto current = lowerBlock(function.body, entry);
  if (current.has_value()) terminate(*current, MirTerminator::returnValue(std::nullopt));
  return result;
}

std::optional<MirBlockId> MirLowerer::lowerBlock(const HirBlock& body, MirBlockId current) {
  std::optional<MirBlockId> active = current;
  for (const auto& statement : body) {
    if (!active.has_value()) break;
    active = lowerStatement(*statement, *active);
  }
  return active;
}

std::optional<MirBlockId> MirLowerer::lowerStatement(const HirStmt& statement,
                                                     MirBlockId current) {
  switch (statement.kind) {
  case HirStmtKind::Binding: {
    const auto& binding = static_cast<const HirBindingStmt&>(statement);
    MirOperand initializer = lowerExpression(*binding.initializer, current);
    const MirLocalId local = localForSymbol(binding.symbol);
    addInstruction(current, MirRvalue::use(std::move(initializer)), local);
    return current;
  }
  case HirStmtKind::Assignment: {
    const auto& assignment = static_cast<const HirAssignmentStmt&>(statement);
    MirOperand value = lowerExpression(*assignment.value, current);
    addInstruction(current, MirRvalue::use(std::move(value)), localForSymbol(assignment.target));
    return current;
  }
  case HirStmtKind::Return: {
    const auto& returned = static_cast<const HirReturnStmt&>(statement);
    std::optional<MirOperand> value;
    if (returned.value) value = lowerExpression(*returned.value, current);
    terminate(current, MirTerminator::returnValue(std::move(value)));
    return std::nullopt;
  }
  case HirStmtKind::Expression: {
    const auto& expression = static_cast<const HirExprStmt&>(statement);
    lowerExpression(*expression.expression, current);
    return current;
  }
  case HirStmtKind::If: {
    const auto& branch = static_cast<const HirIfStmt&>(statement);
    MirOperand condition = lowerExpression(*branch.condition, current);
    const MirBlockId thenBlock = addBlock();
    const MirBlockId elseBlock = addBlock();
    terminate(current, MirTerminator::branch(std::move(condition), thenBlock, elseBlock));
    auto thenEnd = lowerBlock(branch.thenBody, thenBlock);
    auto elseEnd = lowerBlock(branch.elseBody, elseBlock);
    if (!thenEnd.has_value() && !elseEnd.has_value()) return std::nullopt;
    const MirBlockId join = addBlock();
    if (thenEnd.has_value()) terminate(*thenEnd, MirTerminator::goTo(join));
    if (elseEnd.has_value()) terminate(*elseEnd, MirTerminator::goTo(join));
    return join;
  }
  case HirStmtKind::While: {
    const auto& loop = static_cast<const HirWhileStmt&>(statement);
    const MirBlockId conditionBlock = addBlock();
    const MirBlockId bodyBlock = addBlock();
    const MirBlockId exitBlock = addBlock();
    terminate(current, MirTerminator::goTo(conditionBlock));
    MirBlockId conditionEnd = conditionBlock;
    MirOperand condition = lowerExpression(*loop.condition, conditionEnd);
    terminate(conditionEnd,
              MirTerminator::branch(std::move(condition), bodyBlock, exitBlock));
    loops_.push_back({exitBlock, conditionBlock});
    auto bodyEnd = lowerBlock(loop.body, bodyBlock);
    loops_.pop_back();
    if (bodyEnd.has_value()) terminate(*bodyEnd, MirTerminator::goTo(conditionBlock));
    return exitBlock;
  }
  case HirStmtKind::For: {
    const auto& loop = static_cast<const HirForStmt&>(statement);
    MirOperand start = lowerExpression(*loop.start, current);
    MirOperand end = lowerExpression(*loop.end, current);
    const MirLocalId variable = localForSymbol(loop.variable);
    addInstruction(current, MirRvalue::use(std::move(start)), variable);
    end = materialize(current, std::move(end));

    const MirBlockId conditionBlock = addBlock();
    const MirBlockId bodyBlock = addBlock();
    const MirBlockId incrementBlock = addBlock();
    const MirBlockId exitBlock = addBlock();
    terminate(current, MirTerminator::goTo(conditionBlock));

    const MirOperand variableValue = MirOperand::localValue(Type::Int, variable);
    const MirLocalId comparison = addInstruction(
        conditionBlock,
        MirRvalue::binary(Type::Bool, TokenKind::Less, variableValue, end));
    terminate(conditionBlock,
              MirTerminator::branch(MirOperand::localValue(Type::Bool, comparison), bodyBlock,
                                    exitBlock));

    loops_.push_back({exitBlock, incrementBlock});
    auto bodyEnd = lowerBlock(loop.body, bodyBlock);
    loops_.pop_back();
    if (bodyEnd.has_value()) terminate(*bodyEnd, MirTerminator::goTo(incrementBlock));

    const MirLocalId next = addInstruction(
        incrementBlock,
        MirRvalue::binary(Type::Int, TokenKind::Plus,
                          MirOperand::localValue(Type::Int, variable),
                          MirOperand::constantValue(Type::Int, "1")));
    addInstruction(incrementBlock,
                   MirRvalue::use(MirOperand::localValue(Type::Int, next)), variable);
    terminate(incrementBlock, MirTerminator::goTo(conditionBlock));
    return exitBlock;
  }
  case HirStmtKind::Break:
    terminate(current, MirTerminator::goTo(loops_.back().breakTarget));
    return std::nullopt;
  case HirStmtKind::Continue:
    terminate(current, MirTerminator::goTo(loops_.back().continueTarget));
    return std::nullopt;
  }
  return current;
}

MirOperand MirLowerer::lowerExpression(const HirExpr& expression, MirBlockId& current) {
  switch (expression.kind) {
  case HirExprKind::Literal: {
    const auto& literal = static_cast<const HirLiteralExpr&>(expression);
    return MirOperand::constantValue(literal.type, literal.value);
  }
  case HirExprKind::Name: {
    const auto& name = static_cast<const HirNameExpr&>(expression);
    return MirOperand::localValue(name.type, localForSymbol(name.symbol));
  }
  case HirExprKind::Unary: {
    const auto& unary = static_cast<const HirUnaryExpr&>(expression);
    MirOperand operand = lowerExpression(*unary.operand, current);
    const MirLocalId result = addInstruction(
        current, MirRvalue::unary(unary.type, unary.op, std::move(operand)));
    return MirOperand::localValue(unary.type, result);
  }
  case HirExprKind::Binary: {
    const auto& binary = static_cast<const HirBinaryExpr&>(expression);
    if (binary.op == TokenKind::KwAnd || binary.op == TokenKind::KwOr)
      return lowerShortCircuit(binary, current);
    MirOperand left = lowerExpression(*binary.left, current);
    MirOperand right = lowerExpression(*binary.right, current);
    const MirLocalId result = addInstruction(
        current, MirRvalue::binary(binary.type, binary.op, std::move(left), std::move(right)));
    return MirOperand::localValue(binary.type, result);
  }
  case HirExprKind::Call: {
    const auto& call = static_cast<const HirCallExpr&>(expression);
    std::vector<MirOperand> arguments;
    for (const auto& argument : call.arguments)
      arguments.push_back(lowerExpression(*argument, current));
    const MirLocalId result = addInstruction(
        current, MirRvalue::call(call.type, call.callee, std::move(arguments)));
    return MirOperand::localValue(call.type, result);
  }
  }
  return MirOperand::constantValue(Type::Invalid, "");
}

MirOperand MirLowerer::lowerShortCircuit(const HirBinaryExpr& expression,
                                         MirBlockId& current) {
  MirOperand left = lowerExpression(*expression.left, current);
  const MirLocalId result = addLocal(Type::Bool);
  const MirBlockId rightBlock = addBlock();
  const MirBlockId shortBlock = addBlock();
  const MirBlockId joinBlock = addBlock();
  if (expression.op == TokenKind::KwAnd)
    terminate(current, MirTerminator::branch(std::move(left), rightBlock, shortBlock));
  else
    terminate(current, MirTerminator::branch(std::move(left), shortBlock, rightBlock));

  MirBlockId rightEnd = rightBlock;
  MirOperand right = lowerExpression(*expression.right, rightEnd);
  addInstruction(rightEnd, MirRvalue::use(std::move(right)), result);
  terminate(rightEnd, MirTerminator::goTo(joinBlock));

  const std::string shortValue = expression.op == TokenKind::KwAnd ? "false" : "true";
  addInstruction(shortBlock,
                 MirRvalue::use(MirOperand::constantValue(Type::Bool, shortValue)), result);
  terminate(shortBlock, MirTerminator::goTo(joinBlock));
  current = joinBlock;
  return MirOperand::localValue(Type::Bool, result);
}

MirLocalId MirLowerer::addLocal(Type type, SymbolId sourceSymbol, bool parameter) {
  const MirLocalId id = static_cast<MirLocalId>(function_->locals.size());
  function_->locals.push_back({type, sourceSymbol, parameter});
  return id;
}

MirLocalId MirLowerer::localForSymbol(SymbolId symbol) {
  if (symbolLocals_[symbol] == InvalidMirLocal)
    symbolLocals_[symbol] = addLocal(hir_.symbol(symbol).type, symbol, false);
  return symbolLocals_[symbol];
}

MirLocalId MirLowerer::addInstruction(MirBlockId block, MirRvalue value,
                                      MirLocalId destination) {
  if (destination == InvalidMirLocal) destination = addLocal(value.type);
  function_->blocks[block].instructions.push_back({destination, std::move(value)});
  return destination;
}

MirOperand MirLowerer::materialize(MirBlockId block, MirOperand operand) {
  const Type type = operand.type;
  const MirLocalId local = addInstruction(block, MirRvalue::use(std::move(operand)));
  return MirOperand::localValue(type, local);
}

MirBlockId MirLowerer::addBlock() {
  const MirBlockId id = static_cast<MirBlockId>(function_->blocks.size());
  function_->blocks.emplace_back();
  return id;
}

void MirLowerer::terminate(MirBlockId block, MirTerminator terminator) {
  function_->blocks[block].terminator = std::move(terminator);
}

namespace {

bool verifyOperand(const MirOperand& operand, const MirFunction& function, std::string& error) {
  if (operand.type == Type::Invalid) { error = "operand has invalid type"; return false; }
  if (operand.kind == MirOperandKind::Local) {
    if (operand.local >= function.locals.size()) { error = "operand references invalid local"; return false; }
    if (function.locals[operand.local].type != operand.type) { error = "local operand type mismatch"; return false; }
  }
  return true;
}

const char* operandText(const MirOperand& operand, std::ostringstream& out) {
  if (operand.kind == MirOperandKind::Local) out << '%' << operand.local;
  else out << operand.constant;
  return "";
}

} // namespace

bool verifyMir(const MirModule& module, std::string& error) {
  for (const auto& function : module.functions) {
    if (function.symbol >= module.symbols.size()) { error = "function has invalid symbol"; return false; }
    const auto& functionSymbol = module.symbols[function.symbol];
    if (functionSymbol.kind != SymbolKind::Function || functionSymbol.type != function.result) {
      error = "function signature does not match its symbol"; return false;
    }
    if (function.blocks.empty()) { error = "function has no entry block"; return false; }
    if (function.parameters.size() != functionSymbol.parameterTypes.size()) {
      error = "function parameter count does not match its symbol"; return false;
    }
    for (std::size_t parameterIndex = 0; parameterIndex < function.parameters.size();
         ++parameterIndex) {
      const MirLocalId parameter = function.parameters[parameterIndex];
      if (parameter >= function.locals.size() || !function.locals[parameter].parameter) {
        error = "function has invalid parameter local"; return false;
      }
      if (function.locals[parameter].type != functionSymbol.parameterTypes[parameterIndex]) {
        error = "function parameter type does not match its symbol"; return false;
      }
    }
    for (const auto& local : function.locals) {
      if (local.type == Type::Invalid) { error = "local has invalid type"; return false; }
      if (local.sourceSymbol != InvalidSymbol) {
        if (local.sourceSymbol >= module.symbols.size()) {
          error = "local has invalid source symbol"; return false;
        }
        if (module.symbols[local.sourceSymbol].type != local.type) {
          error = "local type does not match its source symbol"; return false;
        }
      }
    }
    for (const auto& block : function.blocks) {
      if (!block.terminator.has_value()) { error = "basic block has no terminator"; return false; }
      for (const auto& instruction : block.instructions) {
        if (instruction.destination >= function.locals.size()) {
          error = "instruction has invalid destination"; return false;
        }
        if (function.locals[instruction.destination].type != instruction.value.type) {
          error = "instruction result type mismatch"; return false;
        }
        if (instruction.value.kind == MirRvalueKind::Use ||
            instruction.value.kind == MirRvalueKind::Unary) {
          if (!verifyOperand(instruction.value.left, function, error)) return false;
          if (instruction.value.kind == MirRvalueKind::Use &&
              instruction.value.left.type != instruction.value.type) {
            error = "use result type mismatch"; return false;
          }
          if (instruction.value.kind == MirRvalueKind::Unary) {
            const bool validNot = instruction.value.op == TokenKind::KwNot &&
                                  instruction.value.left.type == Type::Bool &&
                                  instruction.value.type == Type::Bool;
            const bool validNegate = instruction.value.op == TokenKind::Minus &&
                                     instruction.value.left.type == instruction.value.type &&
                                     (instruction.value.type == Type::Int ||
                                      instruction.value.type == Type::Float);
            if (!validNot && !validNegate) { error = "invalid unary operation"; return false; }
          }
        } else if (instruction.value.kind == MirRvalueKind::Binary) {
          if (!verifyOperand(instruction.value.left, function, error) ||
              !verifyOperand(instruction.value.right, function, error)) return false;
          if (instruction.value.left.type != instruction.value.right.type) {
            error = "binary operand type mismatch"; return false;
          }
          const TokenKind op = instruction.value.op;
          const Type operandType = instruction.value.left.type;
          const bool arithmetic = op == TokenKind::Plus || op == TokenKind::Minus ||
                                  op == TokenKind::Star || op == TokenKind::Slash;
          const bool ordering = op == TokenKind::Less || op == TokenKind::LessEqual ||
                                op == TokenKind::Greater || op == TokenKind::GreaterEqual;
          const bool equality = op == TokenKind::EqualEqual || op == TokenKind::BangEqual;
          const bool validArithmetic = arithmetic && instruction.value.type == operandType &&
                                       (operandType == Type::Int || operandType == Type::Float);
          const bool validComparison = (ordering || equality) &&
                                       instruction.value.type == Type::Bool &&
                                       (!ordering || operandType == Type::Int ||
                                        operandType == Type::Float);
          if (!validArithmetic && !validComparison) {
            error = "invalid binary operation"; return false;
          }
        } else {
          if (instruction.value.callee >= module.symbols.size()) {
            error = "call has invalid callee symbol"; return false;
          }
          const auto kind = module.symbols[instruction.value.callee].kind;
          if (kind != SymbolKind::Function && kind != SymbolKind::BuiltinFunction) {
            error = "call symbol is not callable"; return false;
          }
          for (const auto& argument : instruction.value.arguments)
            if (!verifyOperand(argument, function, error)) return false;
          const auto& callee = module.symbols[instruction.value.callee];
          if (callee.type != instruction.value.type) {
            error = "call result type mismatch"; return false;
          }
          if (kind == SymbolKind::BuiltinFunction) {
            if (instruction.value.arguments.size() != 1 || instruction.value.type != Type::Unit) {
              error = "invalid built-in call"; return false;
            }
          } else {
            if (instruction.value.arguments.size() != callee.parameterTypes.size()) {
              error = "call argument count mismatch"; return false;
            }
            for (std::size_t i = 0; i < instruction.value.arguments.size(); ++i) {
              if (instruction.value.arguments[i].type != callee.parameterTypes[i]) {
                error = "call argument type mismatch"; return false;
              }
            }
          }
        }
      }
      const auto& terminator = *block.terminator;
      if (terminator.kind == MirTerminatorKind::Goto) {
        if (terminator.target >= function.blocks.size()) { error = "goto has invalid target"; return false; }
      } else if (terminator.kind == MirTerminatorKind::Branch) {
        if (!verifyOperand(terminator.condition, function, error)) return false;
        if (terminator.condition.type != Type::Bool) { error = "branch condition is not Bool"; return false; }
        if (terminator.thenTarget >= function.blocks.size() ||
            terminator.elseTarget >= function.blocks.size()) {
          error = "branch has invalid target"; return false;
        }
      } else if (terminator.returned.has_value()) {
        if (!verifyOperand(*terminator.returned, function, error)) return false;
        if (terminator.returned->type != function.result) { error = "return type mismatch"; return false; }
      } else if (function.result != Type::Unit) {
        error = "non-Unit function has empty return"; return false;
      }
    }
  }
  return true;
}

std::string dumpMir(const MirModule& module) {
  std::ostringstream out;
  for (const auto& function : module.functions) {
    out << "fn @" << module.symbols[function.symbol].name << " -> " << typeName(function.result) << "\n";
    for (std::size_t blockId = 0; blockId < function.blocks.size(); ++blockId) {
      out << "bb" << blockId << ":\n";
      const auto& block = function.blocks[blockId];
      for (const auto& instruction : block.instructions) {
        out << "  %" << instruction.destination << " = ";
        switch (instruction.value.kind) {
        case MirRvalueKind::Use:
          out << "use "; operandText(instruction.value.left, out); break;
        case MirRvalueKind::Unary:
          out << tokenName(instruction.value.op) << ' '; operandText(instruction.value.left, out); break;
        case MirRvalueKind::Binary:
          operandText(instruction.value.left, out); out << ' ' << tokenName(instruction.value.op) << ' ';
          operandText(instruction.value.right, out); break;
        case MirRvalueKind::Call:
          out << "call @" << module.symbols[instruction.value.callee].name;
          for (const auto& argument : instruction.value.arguments) { out << ' '; operandText(argument, out); }
          break;
        }
        out << " : " << typeName(instruction.value.type) << "\n";
      }
      const auto& terminator = *block.terminator;
      if (terminator.kind == MirTerminatorKind::Goto) out << "  goto bb" << terminator.target;
      else if (terminator.kind == MirTerminatorKind::Branch) {
        out << "  branch "; operandText(terminator.condition, out);
        out << " -> bb" << terminator.thenTarget << ", bb" << terminator.elseTarget;
      } else {
        out << "  return";
        if (terminator.returned.has_value()) { out << ' '; operandText(*terminator.returned, out); }
      }
      out << "\n";
    }
  }
  return out.str();
}

} // namespace rocket
