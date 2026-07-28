#include "llvm_codegen.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Verifier.h>
#include <llvm/MC/TargetRegistry.h>
#include <llvm/Passes/OptimizationLevel.h>
#include <llvm/Passes/PassBuilder.h>
#include <llvm/Support/FileSystem.h>
#include <llvm/Support/TargetSelect.h>
#include <llvm/Support/raw_ostream.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Target/TargetOptions.h>
#include <llvm/TargetParser/Host.h>

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace rocket {
namespace {

class ModuleLowerer {
public:
  explicit ModuleLowerer(const MirModule& mir)
      : mir_(mir), module_(std::make_unique<llvm::Module>("rocket", context_)),
        builder_(context_), functions_(mir.symbols.size(), nullptr) {}

  bool prepare(bool optimize, std::string& error) {
    if (!createTargetMachine(error) || !lower(error) || !verify(error)) return false;
    if (optimize) {
      optimizeModule();
      if (!verify(error)) return false;
    }
    return true;
  }

  std::string ir() const {
    std::string output;
    llvm::raw_string_ostream stream(output);
    module_->print(stream, nullptr);
    return output;
  }

  bool emit(LlvmFileType fileType, const std::filesystem::path& path,
            std::string& error) {
    std::error_code fileError;
    llvm::raw_fd_ostream output(path.string(), fileError, llvm::sys::fs::OF_None);
    if (fileError) {
      error = "could not open LLVM output '" + path.string() + "': " +
              fileError.message();
      return false;
    }

    const llvm::CodeGenFileType llvmFileType =
        fileType == LlvmFileType::Object ? llvm::CodeGenFileType::ObjectFile
                                         : llvm::CodeGenFileType::AssemblyFile;
    llvm::legacy::PassManager passes;
    if (targetMachine_->addPassesToEmitFile(passes, output, nullptr, llvmFileType)) {
      error = fileType == LlvmFileType::Object
                  ? "LLVM target does not support object emission"
                  : "LLVM target does not support assembly emission";
      return false;
    }
    passes.run(*module_);
    output.flush();
    if (output.has_error()) {
      error = "failed while writing LLVM output '" + path.string() + "'";
      return false;
    }
    return true;
  }

private:
  llvm::Type* valueType(Type type) {
    switch (type) {
    case Type::Int: return llvm::Type::getInt64Ty(context_);
    case Type::Float: return llvm::Type::getDoubleTy(context_);
    case Type::Bool: return llvm::Type::getInt1Ty(context_);
    case Type::Char: return llvm::Type::getInt8Ty(context_);
    case Type::String: return llvm::PointerType::getUnqual(context_);
    case Type::Unit: return llvm::Type::getInt8Ty(context_);
    case Type::Invalid: break;
    }
    return nullptr;
  }

  llvm::Type* resultType(Type type) {
    return type == Type::Unit ? llvm::Type::getVoidTy(context_) : valueType(type);
  }

  llvm::Constant* zero(Type type) {
    switch (type) {
    case Type::Int:
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
    case Type::Float:
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context_), 0.0);
    case Type::Bool:
      return llvm::ConstantInt::getFalse(context_);
    case Type::Char:
    case Type::Unit:
      return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_), 0);
    case Type::String:
      return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    case Type::Invalid: break;
    }
    return nullptr;
  }

  bool createTargetMachine(std::string& error) {
    static const bool initialized = [] {
      return llvm::InitializeNativeTarget() == 0 &&
             llvm::InitializeNativeTargetAsmPrinter() == 0;
    }();
    if (!initialized) {
      error = "LLVM native target initialization failed";
      return false;
    }

    const llvm::Triple triple(llvm::sys::getDefaultTargetTriple());
    std::string targetError;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, targetError);
    if (!target) {
      error = "LLVM target lookup failed: " + targetError;
      return false;
    }

    llvm::TargetOptions options;
    targetMachine_.reset(target->createTargetMachine(
        triple, "x86-64", "", options, std::nullopt, std::nullopt,
        llvm::CodeGenOptLevel::Default));
    if (!targetMachine_) {
      error = "LLVM could not create a target machine for " + triple.str();
      return false;
    }
    module_->setTargetTriple(triple);
    module_->setDataLayout(targetMachine_->createDataLayout());
    return true;
  }

  bool lower(std::string& error) {
    for (const auto& function : mir_.functions) {
      std::vector<llvm::Type*> parameters;
      parameters.reserve(function.parameters.size());
      for (const MirLocalId parameter : function.parameters)
        parameters.push_back(valueType(function.locals[parameter].type));
      auto* signature = llvm::FunctionType::get(resultType(function.result), parameters, false);
      auto* lowered = llvm::Function::Create(
          signature, llvm::Function::ExternalLinkage, functionName(function.symbol), *module_);
      functions_[function.symbol] = lowered;
    }

    for (const auto& function : mir_.functions) {
      if (!lowerFunction(function, error)) return false;
    }
    return lowerEntrypoint(error);
  }

  bool lowerFunction(const MirFunction& function, std::string& error) {
    llvm::Function* lowered = functions_[function.symbol];
    llvm::BasicBlock* prologue = llvm::BasicBlock::Create(context_, "entry", lowered);
    std::vector<llvm::AllocaInst*> locals(function.locals.size(), nullptr);
    builder_.SetInsertPoint(prologue);

    for (MirLocalId id = 0; id < function.locals.size(); ++id) {
      const Type type = function.locals[id].type;
      if (type == Type::Unit) continue;
      locals[id] = builder_.CreateAlloca(valueType(type), nullptr, localName(id));
      builder_.CreateStore(zero(type), locals[id]);
    }

    std::size_t parameterIndex = 0;
    for (llvm::Argument& argument : lowered->args()) {
      const MirLocalId local = function.parameters[parameterIndex++];
      argument.setName("arg." + std::to_string(local));
      if (locals[local]) builder_.CreateStore(&argument, locals[local]);
    }

    std::vector<llvm::BasicBlock*> blocks;
    blocks.reserve(function.blocks.size());
    for (MirBlockId id = 0; id < function.blocks.size(); ++id)
      blocks.push_back(llvm::BasicBlock::Create(context_, "bb" + std::to_string(id), lowered));
    builder_.CreateBr(blocks[0]);

    for (MirBlockId blockId = 0; blockId < function.blocks.size(); ++blockId) {
      builder_.SetInsertPoint(blocks[blockId]);
      for (const auto& instruction : function.blocks[blockId].instructions) {
        llvm::Value* value = lowerRvalue(instruction.value, locals, error);
        if (!value) return false;
        llvm::AllocaInst* destination = locals[instruction.destination];
        if (destination) builder_.CreateStore(value, destination);
      }
      if (!lowerTerminator(*function.blocks[blockId].terminator, function, locals, blocks,
                           error))
        return false;
    }
    return true;
  }

  llvm::Value* lowerOperand(const MirOperand& operand,
                            const std::vector<llvm::AllocaInst*>& locals,
                            std::string& error) {
    if (operand.kind == MirOperandKind::Local) {
      if (operand.type == Type::Unit) return zero(Type::Unit);
      return builder_.CreateLoad(valueType(operand.type), locals[operand.local],
                                 "load." + std::to_string(operand.local));
    }

    switch (operand.type) {
    case Type::Int:
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), operand.constant, 10);
    case Type::Float:
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context_), operand.constant);
    case Type::Bool:
      return llvm::ConstantInt::getBool(context_, operand.constant == "true");
    case Type::Char:
      if (operand.constant.size() != 1) {
        error = "invalid Char constant reached LLVM lowering";
        return nullptr;
      }
      return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_),
                                    static_cast<unsigned char>(operand.constant[0]));
    case Type::String:
      return builder_.CreateGlobalString(operand.constant, "str", 0, module_.get());
    case Type::Unit:
      return zero(Type::Unit);
    case Type::Invalid:
      error = "invalid operand type reached LLVM lowering";
      return nullptr;
    }
    error = "unknown operand type reached LLVM lowering";
    return nullptr;
  }

  llvm::Value* lowerRvalue(const MirRvalue& value,
                           const std::vector<llvm::AllocaInst*>& locals,
                           std::string& error) {
    if (value.kind == MirRvalueKind::Use)
      return lowerOperand(value.left, locals, error);

    if (value.kind == MirRvalueKind::Unary) {
      llvm::Value* operand = lowerOperand(value.left, locals, error);
      if (!operand) return nullptr;
      if (value.op == TokenKind::KwNot) return builder_.CreateNot(operand, "not");
      if (value.type == Type::Float) return builder_.CreateFNeg(operand, "fneg");
      return builder_.CreateNeg(operand, "neg");
    }

    if (value.kind == MirRvalueKind::Binary) {
      llvm::Value* left = lowerOperand(value.left, locals, error);
      llvm::Value* right = lowerOperand(value.right, locals, error);
      if (!left || !right) return nullptr;
      return lowerBinary(value, left, right, error);
    }

    if (mir_.symbols[value.callee].kind == SymbolKind::BuiltinFunction)
      return lowerPrint(value, locals, error);

    std::vector<llvm::Value*> arguments;
    arguments.reserve(value.arguments.size());
    for (const auto& argument : value.arguments) {
      llvm::Value* lowered = lowerOperand(argument, locals, error);
      if (!lowered) return nullptr;
      arguments.push_back(lowered);
    }
    llvm::CallInst* call = builder_.CreateCall(functions_[value.callee], arguments,
                                                value.type == Type::Unit ? "" : "call");
    if (value.type == Type::Unit) return zero(Type::Unit);
    return call;
  }

  llvm::Value* lowerBinary(const MirRvalue& value, llvm::Value* left, llvm::Value* right,
                           std::string& error) {
    const bool floating = value.left.type == Type::Float;
    switch (value.op) {
    case TokenKind::Plus:
      return floating ? builder_.CreateFAdd(left, right, "fadd")
                      : builder_.CreateAdd(left, right, "add");
    case TokenKind::Minus:
      return floating ? builder_.CreateFSub(left, right, "fsub")
                      : builder_.CreateSub(left, right, "sub");
    case TokenKind::Star:
      return floating ? builder_.CreateFMul(left, right, "fmul")
                      : builder_.CreateMul(left, right, "mul");
    case TokenKind::Slash:
      return floating ? builder_.CreateFDiv(left, right, "fdiv")
                      : builder_.CreateSDiv(left, right, "div");
    case TokenKind::EqualEqual:
    case TokenKind::BangEqual:
      return lowerEquality(value.op, value.left.type, left, right);
    case TokenKind::Less:
      return floating ? builder_.CreateFCmpOLT(left, right, "flt")
                      : builder_.CreateICmpSLT(left, right, "lt");
    case TokenKind::LessEqual:
      return floating ? builder_.CreateFCmpOLE(left, right, "fle")
                      : builder_.CreateICmpSLE(left, right, "le");
    case TokenKind::Greater:
      return floating ? builder_.CreateFCmpOGT(left, right, "fgt")
                      : builder_.CreateICmpSGT(left, right, "gt");
    case TokenKind::GreaterEqual:
      return floating ? builder_.CreateFCmpOGE(left, right, "fge")
                      : builder_.CreateICmpSGE(left, right, "ge");
    default:
      error = "unsupported binary operation reached LLVM lowering";
      return nullptr;
    }
  }

  llvm::Value* lowerEquality(TokenKind op, Type type, llvm::Value* left,
                             llvm::Value* right) {
    const bool equal = op == TokenKind::EqualEqual;
    if (type == Type::Unit)
      return llvm::ConstantInt::getBool(context_, equal);
    if (type == Type::Float)
      return equal ? builder_.CreateFCmpOEQ(left, right, "feq")
                   : builder_.CreateFCmpUNE(left, right, "fne");
    if (type == Type::String) {
      llvm::FunctionCallee strcmpFunction = module_->getOrInsertFunction(
          "strcmp", llvm::FunctionType::get(llvm::Type::getInt32Ty(context_),
                                             {llvm::PointerType::getUnqual(context_),
                                              llvm::PointerType::getUnqual(context_)},
                                             false));
      llvm::Value* comparison = builder_.CreateCall(strcmpFunction, {left, right}, "strcmp");
      return equal ? builder_.CreateICmpEQ(comparison, builder_.getInt32(0), "streq")
                   : builder_.CreateICmpNE(comparison, builder_.getInt32(0), "strne");
    }
    return equal ? builder_.CreateICmpEQ(left, right, "eq")
                 : builder_.CreateICmpNE(left, right, "ne");
  }

  llvm::Value* lowerPrint(const MirRvalue& value,
                          const std::vector<llvm::AllocaInst*>& locals,
                          std::string& error) {
    const MirOperand& argument = value.arguments[0];
    llvm::Value* lowered = lowerOperand(argument, locals, error);
    if (!lowered) return nullptr;

    const char* format = nullptr;
    switch (argument.type) {
    case Type::Int: format = "%lld\n"; break;
    case Type::Float: format = "%g\n"; break;
    case Type::Bool:
      format = "%d\n";
      lowered = builder_.CreateZExt(lowered, llvm::Type::getInt32Ty(context_), "bool.print");
      break;
    case Type::Char:
      format = "%c\n";
      lowered = builder_.CreateZExt(lowered, llvm::Type::getInt32Ty(context_), "char.print");
      break;
    case Type::String: format = "%s\n"; break;
    case Type::Unit:
      format = "()\n";
      break;
    case Type::Invalid:
      error = "invalid print argument reached LLVM lowering";
      return nullptr;
    }

    llvm::FunctionCallee printfFunction = module_->getOrInsertFunction(
        "printf", llvm::FunctionType::get(llvm::Type::getInt32Ty(context_),
                                           {llvm::PointerType::getUnqual(context_)}, true));
    llvm::Value* formatString = builder_.CreateGlobalString(format, "fmt", 0, module_.get());
    if (argument.type == Type::Unit)
      builder_.CreateCall(printfFunction, {formatString});
    else
      builder_.CreateCall(printfFunction, {formatString, lowered});
    return zero(Type::Unit);
  }

  bool lowerTerminator(const MirTerminator& terminator, const MirFunction& function,
                       const std::vector<llvm::AllocaInst*>& locals,
                       const std::vector<llvm::BasicBlock*>& blocks, std::string& error) {
    switch (terminator.kind) {
    case MirTerminatorKind::Goto:
      builder_.CreateBr(blocks[terminator.target]);
      return true;
    case MirTerminatorKind::Branch: {
      llvm::Value* condition = lowerOperand(terminator.condition, locals, error);
      if (!condition) return false;
      builder_.CreateCondBr(condition, blocks[terminator.thenTarget],
                            blocks[terminator.elseTarget]);
      return true;
    }
    case MirTerminatorKind::Return:
      if (function.result == Type::Unit) {
        builder_.CreateRetVoid();
        return true;
      }
      if (!terminator.returned.has_value()) {
        error = "non-Unit MIR return reached LLVM lowering without a value";
        return false;
      }
      if (llvm::Value* returned = lowerOperand(*terminator.returned, locals, error)) {
        builder_.CreateRet(returned);
        return true;
      }
      return false;
    }
    return false;
  }

  bool lowerEntrypoint(std::string& error) {
    llvm::Function* rocketMain = nullptr;
    for (const auto& function : mir_.functions) {
      if (mir_.symbols[function.symbol].name == "main") {
        rocketMain = functions_[function.symbol];
        break;
      }
    }
    if (!rocketMain) {
      error = "verified MIR module has no main function";
      return false;
    }

    auto* signature = llvm::FunctionType::get(llvm::Type::getInt32Ty(context_), false);
    auto* entrypoint = llvm::Function::Create(signature, llvm::Function::ExternalLinkage,
                                               "main", *module_);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", entrypoint);
    builder_.SetInsertPoint(entry);
    llvm::Value* result = builder_.CreateCall(rocketMain, {}, "rocket.main");
    builder_.CreateRet(builder_.CreateTrunc(result, llvm::Type::getInt32Ty(context_), "exit"));
    return true;
  }

  bool verify(std::string& error) const {
    std::string verifierOutput;
    llvm::raw_string_ostream stream(verifierOutput);
    if (!llvm::verifyModule(*module_, &stream)) return true;
    error = "LLVM module verification failed: " + stream.str();
    return false;
  }

  void optimizeModule() {
    llvm::LoopAnalysisManager loops;
    llvm::FunctionAnalysisManager functions;
    llvm::CGSCCAnalysisManager cgscc;
    llvm::ModuleAnalysisManager modules;
    llvm::PassBuilder passes(targetMachine_.get());
    passes.registerModuleAnalyses(modules);
    passes.registerCGSCCAnalyses(cgscc);
    passes.registerFunctionAnalyses(functions);
    passes.registerLoopAnalyses(loops);
    passes.crossRegisterProxies(loops, functions, cgscc, modules);
    llvm::ModulePassManager pipeline =
        passes.buildPerModuleDefaultPipeline(llvm::OptimizationLevel::O2);
    pipeline.run(*module_, modules);
  }

  std::string functionName(SymbolId symbol) const {
    return "rocket_fn_" + mir_.symbols[symbol].name + "_" + std::to_string(symbol);
  }

  static std::string localName(MirLocalId local) {
    return "rocket_l_" + std::to_string(local);
  }

  const MirModule& mir_;
  llvm::LLVMContext context_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
  std::vector<llvm::Function*> functions_;
  std::unique_ptr<llvm::TargetMachine> targetMachine_;
};

} // namespace

bool generateLlvmIr(const MirModule& module, bool optimize, std::string& output,
                    std::string& error) {
  output.clear();
  error.clear();
  ModuleLowerer lowerer(module);
  if (!lowerer.prepare(optimize, error)) return false;
  output = lowerer.ir();
  return true;
}

bool emitLlvmFile(const MirModule& module, bool optimize, LlvmFileType fileType,
                  const std::filesystem::path& outputPath, std::string& error) {
  error.clear();
  ModuleLowerer lowerer(module);
  if (!lowerer.prepare(optimize, error)) return false;
  return lowerer.emit(fileType, outputPath, error);
}

} // namespace rocket
