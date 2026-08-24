#include "llvm_codegen.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/BinaryFormat/Dwarf.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
#include <llvm/IR/DIBuilder.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/IR/Intrinsics.h>
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

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace rocket {
namespace {

std::string shortSymbolName(const std::string& name) {
  const std::size_t dot = name.rfind('.');
  return dot == std::string::npos ? name : name.substr(dot + 1);
}

class ModuleLowerer {
public:
  explicit ModuleLowerer(const MirModule& mir, bool debugInfo = false,
                         bool coverage = false, bool profiling = false,
                         Target target = {})
      : mir_(mir), target_(std::move(target)),
        module_(std::make_unique<llvm::Module>("rocket", context_)),
        builder_(context_), functions_(mir.symbols.size(), nullptr),
        callbackWrappers_(mir.symbols.size(), nullptr),
        taskThunks_(mir.symbols.size(), nullptr),
        subprograms_(mir.symbols.size(), nullptr), debugInfo_(debugInfo),
        coverage_(coverage), profiling_(profiling) {}

  bool prepare(bool optimize, std::string& error) {
    if (!createTargetMachine(error)) return false;
    if (debugInfo_) initializeDebug(optimize);
    if (!lower(error)) return false;
    if (debugBuilder_) debugBuilder_->finalize();
    if (!verify(error)) return false;
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
    switch (type.kind) {
    case TypeKind::Int: return llvm::Type::getInt64Ty(context_);
    case TypeKind::Float: return llvm::Type::getDoubleTy(context_);
    case TypeKind::Bool: return llvm::Type::getInt1Ty(context_);
    case TypeKind::Char: return llvm::Type::getInt8Ty(context_);
    case TypeKind::String:
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum:
    case TypeKind::Pointer:
    case TypeKind::NativeStruct:
    case TypeKind::Opaque:
    case TypeKind::Callback:
      return llvm::PointerType::getUnqual(context_);
    case TypeKind::Unit: return llvm::Type::getInt8Ty(context_);
    case TypeKind::TypeParameter:
    case TypeKind::Invalid: break;
    }
    return nullptr;
  }

  llvm::Type* resultType(Type type) {
    return type == Type::Unit ? llvm::Type::getVoidTy(context_) : valueType(type);
  }

  llvm::Constant* zero(Type type) {
    switch (type.kind) {
    case TypeKind::Int:
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0);
    case TypeKind::Float:
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context_), 0.0);
    case TypeKind::Bool:
      return llvm::ConstantInt::getFalse(context_);
    case TypeKind::Char:
    case TypeKind::Unit:
      return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_), 0);
    case TypeKind::String:
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum:
    case TypeKind::Pointer:
    case TypeKind::NativeStruct:
    case TypeKind::Opaque:
    case TypeKind::Callback:
      return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    case TypeKind::TypeParameter:
    case TypeKind::Invalid: break;
    }
    return nullptr;
  }

  llvm::Type* nativeValueType(Type type) {
    if (type == Type::Bool || type == Type::Char)
      return llvm::Type::getInt8Ty(context_);
    if (type == Type::Int) return llvm::Type::getInt64Ty(context_);
    if (type == Type::Float) return llvm::Type::getDoubleTy(context_);
    if (type.kind == TypeKind::Pointer || type.kind == TypeKind::Opaque ||
        type.kind == TypeKind::Callback)
      return llvm::PointerType::getUnqual(context_);
    return type == Type::Unit ? llvm::Type::getVoidTy(context_) : nullptr;
  }

  llvm::FunctionType* nativeFunctionType(const HirSymbol& symbol) {
    std::vector<llvm::Type*> parameters;
    for (const auto& parameter : symbol.parameterTypes)
      parameters.push_back(nativeValueType(parameter));
    return llvm::FunctionType::get(nativeValueType(symbol.type), parameters, false);
  }

  llvm::Value* toNativeValue(llvm::Value* value, Type type) {
    if (type == Type::Bool) return builder_.CreateZExt(value, nativeValueType(type), "abi.bool");
    return value;
  }

  llvm::Value* fromNativeValue(llvm::Value* value, Type type) {
    if (type == Type::Bool) return builder_.CreateTrunc(value, valueType(type), "rocket.bool");
    return value;
  }

  llvm::DIFile* debugFile(const Location& location) {
    std::filesystem::path path = location.file.empty()
                                     ? std::filesystem::path("<generated>")
                                     : std::filesystem::path(location.file);
    path = std::filesystem::absolute(path).lexically_normal();
    const std::string key = path.generic_string();
    if (const auto found = debugFiles_.find(key); found != debugFiles_.end())
      return found->second;
    // Do not embed checkout-specific absolute directories in CodeView/PDB data.
    // Rocket's versioned sidecar map resolves the stable logical file name back
    // to a workspace path for debugger clients.
    auto* file = debugBuilder_->createFile(path.filename().string(),
                                           "rocket://source");
    debugFiles_.emplace(key, file);
    return file;
  }

  llvm::DIType* debugType(const Type& type) {
    if (type == Type::Unit) return nullptr;
    if (type == Type::Int)
      return debugBuilder_->createBasicType("Int", 64, llvm::dwarf::DW_ATE_signed);
    if (type == Type::Float)
      return debugBuilder_->createBasicType("Float", 64, llvm::dwarf::DW_ATE_float);
    if (type == Type::Bool)
      return debugBuilder_->createBasicType("Bool", 8, llvm::dwarf::DW_ATE_boolean);
    if (type == Type::Char)
      return debugBuilder_->createBasicType("Char", 8, llvm::dwarf::DW_ATE_unsigned_char);
    auto* opaque = debugBuilder_->createUnspecifiedType(typeName(type));
    return debugBuilder_->createPointerType(opaque, 64);
  }

  void initializeDebug(bool optimize) {
    debugOptimized_ = optimize;
    debugBuilder_ = std::make_unique<llvm::DIBuilder>(*module_);
    Location first{"<generated>", 1, 1};
    for (const auto& symbol : mir_.symbols)
      if (!symbol.location.file.empty()) { first = symbol.location; break; }
    auto* file = debugFile(first);
    compileUnit_ = debugBuilder_->createCompileUnit(
        llvm::dwarf::DW_LANG_C_plus_plus, file, "Rocket compiler 2.1.0",
        optimize, optimize ? "-O2" : "-O0", 0);
    module_->addModuleFlag(llvm::Module::Warning, "Debug Info Version",
                           llvm::DEBUG_METADATA_VERSION);
    if (target_.operatingSystem == TargetOperatingSystem::Windows)
      module_->addModuleFlag(llvm::Module::Warning, "CodeView", 1);
  }

  llvm::DISubroutineType* debugFunctionType(const HirSymbol& symbol) {
    std::vector<llvm::Metadata*> types;
    types.push_back(debugType(symbol.type));
    for (const auto& parameter : symbol.parameterTypes)
      types.push_back(debugType(parameter));
    return debugBuilder_->createSubroutineType(
        debugBuilder_->getOrCreateTypeArray(types));
  }

  void attachDebugFunction(llvm::Function* function, const HirSymbol& symbol) {
    if (!debugBuilder_ || symbol.location.file.empty()) return;
    auto* file = debugFile(symbol.location);
    auto* subprogram = debugBuilder_->createFunction(
        file, symbol.name, function->getName(), file,
        static_cast<unsigned>(std::max(1, symbol.location.line)),
        debugFunctionType(symbol),
        static_cast<unsigned>(std::max(1, symbol.location.line)),
        llvm::DINode::FlagPrototyped,
        llvm::DISubprogram::SPFlagDefinition |
            (debugOptimized_ ? llvm::DISubprogram::SPFlagOptimized
                             : llvm::DISubprogram::SPFlagZero));
    function->setSubprogram(subprogram);
    if (symbol.id < subprograms_.size()) subprograms_[symbol.id] = subprogram;
  }

  void setDebugLocation(const Location& location, llvm::DIScope* scope) {
    if (!debugBuilder_ || scope == nullptr || location.file.empty()) {
      builder_.SetCurrentDebugLocation(llvm::DebugLoc());
      return;
    }
    builder_.SetCurrentDebugLocation(llvm::DILocation::get(
        context_, std::max(1, location.line), std::max(1, location.column), scope));
  }

  void emitToolingHit(const Location& location, const std::string& symbol,
                      std::uint32_t kind) {
    if (location.file.empty()) return;
    llvm::FunctionCallee hook = module_->getOrInsertFunction(
        "rocket_rt_tooling_hit",
        llvm::FunctionType::get(
            llvm::Type::getVoidTy(context_),
            {llvm::PointerType::getUnqual(context_),
             llvm::Type::getInt64Ty(context_),
             llvm::PointerType::getUnqual(context_),
             llvm::Type::getInt32Ty(context_)}, false));
    llvm::Value* source = builder_.CreateGlobalString(
        std::filesystem::path(location.file).filename().string(), "rocket.source",
        0, module_.get());
    llvm::Value* name = builder_.CreateGlobalString(symbol, "rocket.symbol", 0,
                                                     module_.get());
    builder_.CreateCall(
        hook,
        {source,
         llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_),
                                std::max(1, location.line)),
         name,
         llvm::ConstantInt::get(llvm::Type::getInt32Ty(context_), kind)});
  }

  bool createTargetMachine(std::string& error) {
    static const bool initialized = [] {
      LLVMInitializeX86TargetInfo();
      LLVMInitializeX86Target();
      LLVMInitializeX86TargetMC();
      LLVMInitializeX86AsmParser();
      LLVMInitializeX86AsmPrinter();
      LLVMInitializeAArch64TargetInfo();
      LLVMInitializeAArch64Target();
      LLVMInitializeAArch64TargetMC();
      LLVMInitializeAArch64AsmParser();
      LLVMInitializeAArch64AsmPrinter();
      return true;
    }();
    if (!initialized) {
      error = "LLVM native target initialization failed";
      return false;
    }

    const llvm::Triple triple(llvm::Triple::normalize(target_.triple));
    std::string targetError;
    const llvm::Target* target = llvm::TargetRegistry::lookupTarget(triple, targetError);
    if (!target) {
      error = "LLVM target lookup failed: " + targetError;
      return false;
    }

    llvm::TargetOptions options;
    const std::string cpu =
        target_.architecture == TargetArchitecture::X64 ? "x86-64" : "generic";
    targetMachine_.reset(target->createTargetMachine(
        triple, cpu, "", options, std::nullopt, std::nullopt,
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
      attachDebugFunction(lowered, mir_.symbols[function.symbol]);
    }

    for (const auto& symbol : mir_.symbols) {
      if (!symbol.nativeImport) continue;
      if (module_->getFunction(symbol.nativeName)) {
        error = "duplicate native C symbol '" + symbol.nativeName + "'";
        return false;
      }
      functions_[symbol.id] = llvm::Function::Create(
          nativeFunctionType(symbol), llvm::Function::ExternalLinkage,
          symbol.nativeName, *module_);
    }

    for (const auto& function : mir_.functions) {
      if (!lowerFunction(function, error)) return false;
    }
    if (!lowerNativeExports(error)) return false;
    return mir_.library || lowerEntrypoint(error);
  }

  llvm::Function* callbackWrapper(SymbolId target, Type callbackType,
                                  std::string& error) {
    if (target >= callbackWrappers_.size() || !functions_[target]) {
      error = "invalid Rocket callback target";
      return nullptr;
    }
    if (callbackWrappers_[target]) return callbackWrappers_[target];
    const HirSymbol& symbol = mir_.symbols[target];
    auto* signature = nativeFunctionType(symbol);
    auto* wrapper = llvm::Function::Create(
        signature, llvm::Function::InternalLinkage,
        "rocket_callback_" + std::to_string(target), *module_);
    callbackWrappers_[target] = wrapper;
    const auto saved = builder_.saveIP();
    auto* entry = llvm::BasicBlock::Create(context_, "entry", wrapper);
    builder_.SetInsertPoint(entry);
    std::vector<llvm::Value*> arguments;
    std::size_t index = 0;
    for (llvm::Argument& argument : wrapper->args())
      arguments.push_back(fromNativeValue(&argument, symbol.parameterTypes[index++]));
    llvm::CallInst* call = builder_.CreateCall(functions_[target], arguments,
                                                symbol.type == Type::Unit ? "" : "callback");
    if (symbol.type == Type::Unit)
      builder_.CreateRetVoid();
    else
      builder_.CreateRet(toNativeValue(call, symbol.type));
    builder_.restoreIP(saved);
    (void)callbackType;
    return wrapper;
  }

  bool lowerNativeExports(std::string& error) {
    for (const auto& symbol : mir_.symbols) {
      if (!symbol.nativeExport) continue;
      if (!functions_[symbol.id]) {
        error = "native export has no Rocket implementation";
        return false;
      }
      if (module_->getFunction(symbol.nativeName)) {
        error = "duplicate native C symbol '" + symbol.nativeName + "'";
        return false;
      }
      auto* wrapper = llvm::Function::Create(
          nativeFunctionType(symbol), llvm::Function::ExternalLinkage,
          symbol.nativeName, *module_);
      if (target_.operatingSystem == TargetOperatingSystem::Windows)
        wrapper->setDLLStorageClass(llvm::GlobalValue::DLLExportStorageClass);
      auto* entry = llvm::BasicBlock::Create(context_, "entry", wrapper);
      builder_.SetInsertPoint(entry);
      std::vector<llvm::Value*> arguments;
      std::size_t index = 0;
      for (llvm::Argument& argument : wrapper->args())
        arguments.push_back(fromNativeValue(&argument, symbol.parameterTypes[index++]));
      llvm::CallInst* call = builder_.CreateCall(functions_[symbol.id], arguments,
                                                  symbol.type == Type::Unit ? "" : "export");
      if (symbol.type == Type::Unit)
        builder_.CreateRetVoid();
      else
        builder_.CreateRet(toNativeValue(call, symbol.type));
    }
    return true;
  }

  bool lowerFunction(const MirFunction& function, std::string& error) {
    llvm::Function* lowered = functions_[function.symbol];
    llvm::DISubprogram* debugScope = function.symbol < subprograms_.size()
                                          ? subprograms_[function.symbol]
                                          : nullptr;
    llvm::BasicBlock* prologue = llvm::BasicBlock::Create(context_, "entry", lowered);
    std::vector<llvm::AllocaInst*> locals(function.locals.size(), nullptr);
    builder_.SetInsertPoint(prologue);
    setDebugLocation(mir_.symbols[function.symbol].location, debugScope);
    if (profiling_)
      emitToolingHit(mir_.symbols[function.symbol].location,
                     functions_[function.symbol]->getName().str(), 2);

    for (MirLocalId id = 0; id < function.locals.size(); ++id) {
      const Type type = function.locals[id].type;
      if (type == Type::Unit) continue;
      std::string name = localName(id);
      const SymbolId sourceSymbol = function.locals[id].sourceSymbol;
      if (sourceSymbol != InvalidSymbol && sourceSymbol < mir_.symbols.size())
        name = mir_.symbols[sourceSymbol].name;
      locals[id] = builder_.CreateAlloca(valueType(type), nullptr, name);
      builder_.CreateStore(zero(type), locals[id]);
      if (debugBuilder_ && debugScope != nullptr && sourceSymbol != InvalidSymbol &&
          sourceSymbol < mir_.symbols.size()) {
        const auto& symbol = mir_.symbols[sourceSymbol];
        auto* variable = debugBuilder_->createAutoVariable(
            debugScope, shortSymbolName(symbol.name), debugFile(symbol.location),
            std::max(1, symbol.location.line), debugType(type), true);
        debugBuilder_->insertDeclare(
            locals[id], variable, debugBuilder_->createExpression(),
            llvm::DILocation::get(context_, std::max(1, symbol.location.line),
                                  std::max(1, symbol.location.column), debugScope),
            prologue);
      }
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
        setDebugLocation(instruction.location, debugScope);
        if (coverage_)
          emitToolingHit(instruction.location,
                         functions_[function.symbol]->getName().str(), 1);
        if (instruction.kind != MirInstructionKind::Assign) {
          llvm::Value* object = lowerOperand(instruction.arcOperand, locals, error);
          if (!object) return false;
          const char* name = instruction.kind == MirInstructionKind::Retain
                                 ? "rocket_rt_retain"
                                 : "rocket_rt_release";
          llvm::FunctionCallee operation = module_->getOrInsertFunction(
              name, llvm::FunctionType::get(llvm::Type::getVoidTy(context_),
                                             {llvm::PointerType::getUnqual(context_)}, false));
          builder_.CreateCall(operation, {object});
          continue;
        }
        llvm::Value* value = lowerRvalue(instruction.value, locals, error);
        if (!value) return false;
        llvm::AllocaInst* destination = locals[instruction.destination];
        if (destination) builder_.CreateStore(value, destination);
      }
      setDebugLocation(function.blocks[blockId].terminator->location, debugScope);
      if (!lowerTerminator(*function.blocks[blockId].terminator, function, locals, blocks,
                           error))
        return false;
    }
    builder_.SetCurrentDebugLocation(llvm::DebugLoc());
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

    switch (operand.type.kind) {
    case TypeKind::Int:
      return llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), operand.constant, 10);
    case TypeKind::Float:
      return llvm::ConstantFP::get(llvm::Type::getDoubleTy(context_), operand.constant);
    case TypeKind::Bool:
      return llvm::ConstantInt::getBool(context_, operand.constant == "true");
    case TypeKind::Char:
      if (operand.constant.size() != 1) {
        error = "invalid Char constant reached LLVM lowering";
        return nullptr;
      }
      return llvm::ConstantInt::get(llvm::Type::getInt8Ty(context_),
                                    static_cast<unsigned char>(operand.constant[0]));
    case TypeKind::String:
    {
      llvm::Value* bytes = builder_.CreateGlobalString(operand.constant, "str", 0, module_.get());
      llvm::FunctionCallee constructor = module_->getOrInsertFunction(
          "rocket_rt_string_new",
          llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                  {llvm::PointerType::getUnqual(context_),
                                   llvm::Type::getInt64Ty(context_)},
                                  false));
      return builder_.CreateCall(
          constructor,
          {bytes, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_),
                                         operand.constant.size())},
          "string.new");
    }
    case TypeKind::Unit:
      return zero(Type::Unit);
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum:
    case TypeKind::Pointer:
    case TypeKind::NativeStruct:
    case TypeKind::Opaque:
      error = "aggregate constant reached LLVM lowering";
      return nullptr;
    case TypeKind::Callback: {
      SymbolId target = InvalidSymbol;
      try {
        target = static_cast<SymbolId>(std::stoul(operand.constant));
      } catch (...) {
        error = "invalid callback constant reached LLVM lowering";
        return nullptr;
      }
      return callbackWrapper(target, operand.type, error);
    }
    case TypeKind::TypeParameter:
    case TypeKind::Invalid:
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
      return lowerCheckedInteger(
          llvm::Intrinsic::ssub_with_overflow,
          llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0), operand, "neg");
    }

    if (value.kind == MirRvalueKind::Binary) {
      llvm::Value* left = lowerOperand(value.left, locals, error);
      llvm::Value* right = lowerOperand(value.right, locals, error);
      if (!left || !right) return nullptr;
      return lowerBinary(value, left, right, error);
    }

    if (value.kind == MirRvalueKind::Array)
      return lowerArray(value, locals, error);

    if (value.kind == MirRvalueKind::ArrayUpdate)
      return lowerArrayUpdate(value, locals, error);

    if (value.kind == MirRvalueKind::Index)
      return lowerIndex(value, locals, error);

    if (value.kind == MirRvalueKind::Slice)
      return lowerSlice(value, locals, error);

    if (value.kind == MirRvalueKind::Aggregate)
      return lowerAggregate(value, locals, error);

    if (value.kind == MirRvalueKind::Field)
      return lowerField(value, locals, error);

    if (value.kind == MirRvalueKind::Tag)
      return lowerTag(value, locals, error);

    if (value.kind == MirRvalueKind::AsyncCall)
      return lowerAsyncCall(value, locals, error);

    if (value.kind == MirRvalueKind::Await) {
      llvm::Value* task = lowerOperand(value.left, locals, error);
      if (!task) return nullptr;
      llvm::FunctionCallee await = module_->getOrInsertFunction(
          "rocket_rt_task_await",
          llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                  {llvm::PointerType::getUnqual(context_)}, false));
      return builder_.CreateCall(await, {task}, "task.await");
    }

    if (mir_.symbols[value.callee].kind == SymbolKind::BuiltinFunction) {
      if (mir_.symbols[value.callee].intrinsic == Intrinsic::Print)
        return lowerPrint(value, locals, error);
      return lowerStandard(value, locals, error);
    }

    const HirSymbol& callee = mir_.symbols[value.callee];
    std::vector<llvm::Value*> arguments;
    arguments.reserve(value.arguments.size());
    for (const auto& argument : value.arguments) {
      llvm::Value* lowered = lowerOperand(argument, locals, error);
      if (!lowered) return nullptr;
      const std::size_t index = arguments.size();
      arguments.push_back(callee.nativeImport
                              ? toNativeValue(lowered, callee.parameterTypes[index])
                              : lowered);
    }
    llvm::CallInst* call = builder_.CreateCall(functions_[value.callee], arguments,
                                                value.type == Type::Unit ? "" : "call");
    if (value.type == Type::Unit) return zero(Type::Unit);
    return callee.nativeImport ? fromNativeValue(call, value.type) : call;
  }

  llvm::Function* asyncThunk(SymbolId target, std::string& error) {
    if (target >= taskThunks_.size() || !functions_[target]) {
      error = "invalid async task target";
      return nullptr;
    }
    if (taskThunks_[target]) return taskThunks_[target];
    auto* pointer = llvm::PointerType::getUnqual(context_);
    auto* signature = llvm::FunctionType::get(pointer, {pointer}, false);
    auto* thunk = llvm::Function::Create(
        signature, llvm::Function::InternalLinkage,
        "rocket_async_entry_" + std::to_string(target), *module_);
    taskThunks_[target] = thunk;
    const auto saved = builder_.saveIP();
    auto* entry = llvm::BasicBlock::Create(context_, "entry", thunk);
    builder_.SetInsertPoint(entry);
    llvm::Value* context = thunk->arg_begin();
    context->setName("task.context");
    const HirSymbol& symbol = mir_.symbols[target];
    std::vector<llvm::Value*> arguments;
    arguments.reserve(symbol.parameterTypes.size());
    for (std::size_t index = 0; index < symbol.parameterTypes.size(); ++index) {
      const Type& type = symbol.parameterTypes[index];
      const char* suffix = aggregateSuffix(type);
      if (std::string_view(suffix) == "invalid") {
        builder_.restoreIP(saved);
        error = "async task has an unsupported captured parameter type";
        return nullptr;
      }
      llvm::FunctionCallee getter = module_->getOrInsertFunction(
          std::string("rocket_rt_aggregate_get_") + suffix,
          llvm::FunctionType::get(aggregateRuntimeType(type),
                                  {pointer, llvm::Type::getInt32Ty(context_)}, false));
      llvm::Value* argument = builder_.CreateCall(
          getter, {context, builder_.getInt32(static_cast<std::uint32_t>(index))},
          "task.argument");
      if (type == Type::Bool)
        argument = builder_.CreateTrunc(argument, llvm::Type::getInt1Ty(context_),
                                        "task.argument.bool");
      arguments.push_back(argument);
    }
    llvm::Value* result = builder_.CreateCall(functions_[target], arguments, "task.result");
    builder_.CreateRet(result);
    builder_.restoreIP(saved);
    return thunk;
  }

  llvm::Value* lowerAsyncCall(const MirRvalue& value,
                              const std::vector<llvm::AllocaInst*>& locals,
                              std::string& error) {
    MirRvalue captured = MirRvalue::aggregate(Type::Invalid, 0, 0, value.arguments);
    llvm::Value* context = lowerAggregate(captured, locals, error);
    if (!context) return nullptr;
    llvm::Function* entry = asyncThunk(value.callee, error);
    if (!entry) return nullptr;
    auto* pointer = llvm::PointerType::getUnqual(context_);
    llvm::FunctionCallee spawn = module_->getOrInsertFunction(
        "rocket_rt_task_spawn",
        llvm::FunctionType::get(pointer, {pointer, pointer}, false));
    return builder_.CreateCall(spawn, {entry, context}, "task.spawn");
  }

  static std::uint32_t runtimeElementKind(Type element) {
    switch (element.kind) {
    case TypeKind::Int: return 1;
    case TypeKind::Float: return 2;
    case TypeKind::Bool: return 3;
    case TypeKind::Char: return 4;
    case TypeKind::String: return 5;
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum: return 6;
    default: return 0;
    }
  }

  static const char* runtimeElementSuffix(Type element) {
    switch (element.kind) {
    case TypeKind::Int: return "int";
    case TypeKind::Float: return "float";
    case TypeKind::Bool: return "bool";
    case TypeKind::Char: return "char";
    case TypeKind::String: return "string";
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum: return "managed";
    default: return "invalid";
    }
  }

  llvm::Type* runtimeElementType(Type element) {
    return element == Type::Bool ? llvm::Type::getInt8Ty(context_) : valueType(element);
  }

  static const char* aggregateSuffix(const Type& type) {
    switch (type.kind) {
    case TypeKind::Int: return "int";
    case TypeKind::Float: return "float";
    case TypeKind::Bool: return "bool";
    case TypeKind::Char: return "char";
    case TypeKind::String:
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum: return "managed";
    default: return "invalid";
    }
  }

  llvm::Type* aggregateRuntimeType(const Type& type) {
    return type == Type::Bool ? llvm::Type::getInt8Ty(context_) : valueType(type);
  }

  bool lowerCollectionOperands(const MirRvalue& value,
                               const std::vector<llvm::AllocaInst*>& locals,
                               llvm::Value*& collection, llvm::Value*& first,
                               llvm::Value*& second, std::string& error) {
    collection = lowerOperand(value.left, locals, error);
    first = lowerOperand(value.right, locals, error);
    second = value.kind == MirRvalueKind::Slice
                 ? lowerOperand(value.end, locals, error)
                 : nullptr;
    return collection && first && (value.kind != MirRvalueKind::Slice || second);
  }

  llvm::Value* lowerArray(const MirRvalue& value,
                          const std::vector<llvm::AllocaInst*>& locals,
                          std::string& error) {
    const Type element = collectionElementType(value.type);
    llvm::FunctionCallee constructor = module_->getOrInsertFunction(
        "rocket_rt_array_new",
        llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                {llvm::Type::getInt32Ty(context_),
                                 llvm::Type::getInt64Ty(context_)}, false));
    llvm::Value* array = builder_.CreateCall(
        constructor,
        {builder_.getInt32(runtimeElementKind(element)),
         llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), value.arguments.size())},
        "array.new");

    const std::string setterName =
        std::string("rocket_rt_array_set_") + runtimeElementSuffix(element);
    llvm::FunctionCallee setter = module_->getOrInsertFunction(
        setterName,
        llvm::FunctionType::get(llvm::Type::getVoidTy(context_),
                                {llvm::PointerType::getUnqual(context_),
                                 llvm::Type::getInt64Ty(context_),
                                 runtimeElementType(element)}, false));
    for (std::size_t index = 0; index < value.arguments.size(); ++index) {
      llvm::Value* elementValue = lowerOperand(value.arguments[index], locals, error);
      if (!elementValue) return nullptr;
      if (element == Type::Bool)
        elementValue = builder_.CreateZExt(elementValue, llvm::Type::getInt8Ty(context_),
                                           "array.bool");
      builder_.CreateCall(
          setter,
          {array, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), index),
           elementValue});
    }
    return array;
  }

  llvm::Value* lowerIndex(const MirRvalue& value,
                          const std::vector<llvm::AllocaInst*>& locals,
                          std::string& error) {
    llvm::Value* collection = nullptr;
    llvm::Value* index = nullptr;
    llvm::Value* unused = nullptr;
    if (!lowerCollectionOperands(value, locals, collection, index, unused, error))
      return nullptr;
    const std::string functionName =
        std::string("rocket_rt_index_") + runtimeElementSuffix(value.type);
    llvm::FunctionCallee getter = module_->getOrInsertFunction(
        functionName,
        llvm::FunctionType::get(runtimeElementType(value.type),
                                {llvm::PointerType::getUnqual(context_),
                                 llvm::Type::getInt64Ty(context_)}, false));
    llvm::Value* result = builder_.CreateCall(getter, {collection, index}, "index");
    if (value.type == Type::Bool)
      return builder_.CreateTrunc(result, llvm::Type::getInt1Ty(context_), "index.bool");
    return result;
  }

  llvm::Value* lowerArrayUpdate(const MirRvalue& value,
                                const std::vector<llvm::AllocaInst*>& locals,
                                std::string& error) {
    llvm::Value* array = lowerOperand(value.left, locals, error);
    llvm::Value* index = lowerOperand(value.right, locals, error);
    llvm::Value* elementValue = lowerOperand(value.end, locals, error);
    if (!array || !index || !elementValue) return nullptr;
    const Type element = collectionElementType(value.type);
    if (element == Type::Bool)
      elementValue = builder_.CreateZExt(elementValue,
                                         llvm::Type::getInt8Ty(context_),
                                         "array.update.bool");
    const std::string functionName =
        std::string("rocket_rt_array_update_") + runtimeElementSuffix(element);
    llvm::FunctionCallee update = module_->getOrInsertFunction(
        functionName,
        llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                {llvm::PointerType::getUnqual(context_),
                                 llvm::Type::getInt64Ty(context_),
                                 runtimeElementType(element)}, false));
    return builder_.CreateCall(update, {array, index, elementValue},
                               "array.update");
  }

  llvm::Value* lowerSlice(const MirRvalue& value,
                          const std::vector<llvm::AllocaInst*>& locals,
                          std::string& error) {
    llvm::Value* collection = nullptr;
    llvm::Value* start = nullptr;
    llvm::Value* end = nullptr;
    if (!lowerCollectionOperands(value, locals, collection, start, end, error))
      return nullptr;
    llvm::FunctionCallee constructor = module_->getOrInsertFunction(
        "rocket_rt_slice_new",
        llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                {llvm::PointerType::getUnqual(context_),
                                 llvm::Type::getInt64Ty(context_),
                                 llvm::Type::getInt64Ty(context_)}, false));
    return builder_.CreateCall(constructor, {collection, start, end}, "slice.new");
  }

  llvm::Value* lowerAggregate(const MirRvalue& value,
                              const std::vector<llvm::AllocaInst*>& locals,
                              std::string& error) {
    std::uint64_t managedMask = 0;
    for (std::size_t index = 0; index < value.arguments.size(); ++index)
      if (isManagedType(value.arguments[index].type)) managedMask |= std::uint64_t{1} << index;
    llvm::FunctionCallee constructor = module_->getOrInsertFunction(
        "rocket_rt_aggregate_new",
        llvm::FunctionType::get(llvm::PointerType::getUnqual(context_),
                                {llvm::Type::getInt32Ty(context_),
                                 llvm::Type::getInt32Ty(context_),
                                 llvm::Type::getInt64Ty(context_)}, false));
    llvm::Value* aggregate = builder_.CreateCall(
        constructor,
        {builder_.getInt32(value.tag),
         builder_.getInt32(static_cast<std::uint32_t>(value.arguments.size())),
         llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), managedMask)},
        "aggregate.new");
    for (std::size_t index = 0; index < value.arguments.size(); ++index) {
      const Type& fieldType = value.arguments[index].type;
      llvm::Value* field = lowerOperand(value.arguments[index], locals, error);
      if (!field) return nullptr;
      if (fieldType == Type::Bool)
        field = builder_.CreateZExt(field, llvm::Type::getInt8Ty(context_),
                                    "aggregate.bool");
      const std::string setterName =
          std::string("rocket_rt_aggregate_set_") + aggregateSuffix(fieldType);
      llvm::FunctionCallee setter = module_->getOrInsertFunction(
          setterName,
          llvm::FunctionType::get(llvm::Type::getVoidTy(context_),
                                  {llvm::PointerType::getUnqual(context_),
                                   llvm::Type::getInt32Ty(context_),
                                   aggregateRuntimeType(fieldType)}, false));
      builder_.CreateCall(setter,
                          {aggregate, builder_.getInt32(static_cast<std::uint32_t>(index)),
                           field});
    }
    return aggregate;
  }

  llvm::Value* lowerField(const MirRvalue& value,
                          const std::vector<llvm::AllocaInst*>& locals,
                          std::string& error) {
    llvm::Value* aggregate = lowerOperand(value.left, locals, error);
    if (!aggregate) return nullptr;
    const std::string getterName =
        std::string("rocket_rt_aggregate_get_") + aggregateSuffix(value.type);
    llvm::FunctionCallee getter = module_->getOrInsertFunction(
        getterName,
        llvm::FunctionType::get(aggregateRuntimeType(value.type),
                                {llvm::PointerType::getUnqual(context_),
                                 llvm::Type::getInt32Ty(context_)}, false));
    llvm::Value* result = builder_.CreateCall(
        getter, {aggregate, builder_.getInt32(value.tag)}, "aggregate.field");
    if (value.type == Type::Bool)
      return builder_.CreateTrunc(result, llvm::Type::getInt1Ty(context_),
                                  "aggregate.field.bool");
    return result;
  }

  llvm::Value* lowerTag(const MirRvalue& value,
                        const std::vector<llvm::AllocaInst*>& locals,
                        std::string& error) {
    llvm::Value* aggregate = lowerOperand(value.left, locals, error);
    if (!aggregate) return nullptr;
    llvm::FunctionCallee getter = module_->getOrInsertFunction(
        "rocket_rt_aggregate_tag",
        llvm::FunctionType::get(llvm::Type::getInt32Ty(context_),
                                {llvm::PointerType::getUnqual(context_)}, false));
    llvm::Value* tag = builder_.CreateCall(getter, {aggregate}, "aggregate.tag");
    return builder_.CreateZExt(tag, llvm::Type::getInt64Ty(context_), "aggregate.tag.int");
  }

  llvm::Value* lowerBinary(const MirRvalue& value, llvm::Value* left, llvm::Value* right,
                           std::string& error) {
    const bool floating = value.left.type == Type::Float;
    switch (value.op) {
    case TokenKind::Plus:
      return floating ? builder_.CreateFAdd(left, right, "fadd")
                      : lowerCheckedInteger(llvm::Intrinsic::sadd_with_overflow,
                                            left, right, "add");
    case TokenKind::Minus:
      return floating ? builder_.CreateFSub(left, right, "fsub")
                      : lowerCheckedInteger(llvm::Intrinsic::ssub_with_overflow,
                                            left, right, "sub");
    case TokenKind::Star:
      return floating ? builder_.CreateFMul(left, right, "fmul")
                      : lowerCheckedInteger(llvm::Intrinsic::smul_with_overflow,
                                            left, right, "mul");
    case TokenKind::Slash:
      return floating ? builder_.CreateFDiv(left, right, "fdiv")
                      : lowerCheckedDivision(left, right);
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

  void guardRuntimeFailure(llvm::Value* failed, const char* runtimeFunction,
                           const char* label) {
    llvm::Function* function = builder_.GetInsertBlock()->getParent();
    llvm::BasicBlock* failure = llvm::BasicBlock::Create(
        context_, std::string(label) + ".fail", function);
    llvm::BasicBlock* continuation = llvm::BasicBlock::Create(
        context_, std::string(label) + ".ok", function);
    builder_.CreateCondBr(failed, failure, continuation);
    builder_.SetInsertPoint(failure);
    llvm::FunctionCallee panic = module_->getOrInsertFunction(
        runtimeFunction,
        llvm::FunctionType::get(llvm::Type::getVoidTy(context_), false));
    builder_.CreateCall(panic, {});
    builder_.CreateUnreachable();
    builder_.SetInsertPoint(continuation);
  }

  llvm::Value* lowerCheckedInteger(llvm::Intrinsic::ID intrinsic, llvm::Value* left,
                                   llvm::Value* right, const char* name) {
    llvm::Function* operation = llvm::Intrinsic::getOrInsertDeclaration(
        module_.get(), intrinsic, {llvm::Type::getInt64Ty(context_)});
    llvm::Value* pair = builder_.CreateCall(operation, {left, right},
                                            std::string(name) + ".checked");
    llvm::Value* result = builder_.CreateExtractValue(pair, {0}, name);
    llvm::Value* overflow = builder_.CreateExtractValue(pair, {1},
                                                        std::string(name) + ".overflow");
    guardRuntimeFailure(overflow, "rocket_rt_panic_integer_overflow", name);
    return result;
  }

  llvm::Value* lowerCheckedDivision(llvm::Value* left, llvm::Value* right) {
    llvm::Value* zero = builder_.CreateICmpEQ(
        right, llvm::ConstantInt::get(llvm::Type::getInt64Ty(context_), 0), "div.zero");
    guardRuntimeFailure(zero, "rocket_rt_panic_division_by_zero", "div.zero");
    llvm::Value* minimum = builder_.CreateICmpEQ(
        left, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context_),
                                           std::numeric_limits<std::int64_t>::min()),
        "div.minimum");
    llvm::Value* negativeOne = builder_.CreateICmpEQ(
        right, llvm::ConstantInt::getSigned(llvm::Type::getInt64Ty(context_), -1),
        "div.negative_one");
    guardRuntimeFailure(builder_.CreateAnd(minimum, negativeOne, "div.overflow"),
                        "rocket_rt_panic_integer_overflow", "div.overflow");
    return builder_.CreateSDiv(left, right, "div");
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
      llvm::FunctionCallee equality = module_->getOrInsertFunction(
          "rocket_rt_string_equal",
          llvm::FunctionType::get(llvm::Type::getInt8Ty(context_),
                                  {llvm::PointerType::getUnqual(context_),
                                   llvm::PointerType::getUnqual(context_)},
                                  false));
      llvm::Value* comparison = builder_.CreateCall(equality, {left, right}, "string.equal");
      llvm::Value* isEqual = builder_.CreateICmpNE(comparison, builder_.getInt8(0), "streq");
      return equal ? isEqual : builder_.CreateNot(isEqual, "strne");
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

    const char* runtimeFunction = nullptr;
    llvm::Type* runtimeArgumentType = nullptr;
    switch (argument.type.kind) {
    case TypeKind::Int:
      runtimeFunction = "rocket_rt_print_int";
      runtimeArgumentType = llvm::Type::getInt64Ty(context_);
      break;
    case TypeKind::Float:
      runtimeFunction = "rocket_rt_print_float";
      runtimeArgumentType = llvm::Type::getDoubleTy(context_);
      break;
    case TypeKind::Bool:
      runtimeFunction = "rocket_rt_print_bool";
      runtimeArgumentType = llvm::Type::getInt8Ty(context_);
      lowered = builder_.CreateZExt(lowered, runtimeArgumentType, "bool.print");
      break;
    case TypeKind::Char:
      runtimeFunction = "rocket_rt_print_char";
      runtimeArgumentType = llvm::Type::getInt8Ty(context_);
      break;
    case TypeKind::String:
      runtimeFunction = "rocket_rt_print_string";
      runtimeArgumentType = llvm::PointerType::getUnqual(context_);
      break;
    case TypeKind::Unit:
      runtimeFunction = "rocket_rt_print_unit";
      break;
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Weak:
    case TypeKind::UniqueBuffer:
    case TypeKind::Task:
    case TypeKind::Struct:
    case TypeKind::Enum:
      error = "aggregate print reached LLVM lowering";
      return nullptr;
    case TypeKind::TypeParameter:
    case TypeKind::Invalid:
      error = "invalid print argument reached LLVM lowering";
      return nullptr;
    }

    if (argument.type == Type::Unit) {
      llvm::FunctionCallee print = module_->getOrInsertFunction(
          runtimeFunction, llvm::FunctionType::get(llvm::Type::getVoidTy(context_), false));
      builder_.CreateCall(print, {});
    } else {
      llvm::FunctionCallee print = module_->getOrInsertFunction(
          runtimeFunction,
          llvm::FunctionType::get(llvm::Type::getVoidTy(context_),
                                  {runtimeArgumentType}, false));
      builder_.CreateCall(print, {lowered});
    }
    return zero(Type::Unit);
  }

  static const char* standardRuntimeName(Intrinsic intrinsic) {
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
    case Intrinsic::CollectionsCapacity: return "rocket_std_collections_capacity";
    case Intrinsic::CollectionsReserve: return "rocket_std_collections_reserve";
    case Intrinsic::CollectionsAppend: return "rocket_std_collections_append";
    case Intrinsic::CollectionsPop: return "rocket_std_collections_pop";
    case Intrinsic::CollectionsInsert: return "rocket_std_collections_insert";
    case Intrinsic::CollectionsRemove: return "rocket_std_collections_remove";
    case Intrinsic::CollectionsClear: return "rocket_std_collections_clear";
    case Intrinsic::CollectionsMapFromArrays: return "rocket_std_collections_map_from_arrays";
    case Intrinsic::CollectionsMapLength: return "rocket_std_collections_map_length";
    case Intrinsic::CollectionsMapFind: return "rocket_std_collections_map_find";
    case Intrinsic::CollectionsMapGet: return "rocket_std_collections_map_get";
    case Intrinsic::CollectionsMapKeys: return "rocket_std_collections_map_keys";
    case Intrinsic::CollectionsMapValues: return "rocket_std_collections_map_values";
    case Intrinsic::CollectionsSetFromArray: return "rocket_std_collections_set_from_array";
    case Intrinsic::CollectionsSetContains: return "rocket_std_collections_set_contains";
    case Intrinsic::CollectionsSetValues: return "rocket_std_collections_set_values";
    case Intrinsic::CollectionsHash: return "rocket_std_collections_hash";
    case Intrinsic::CollectionsContains: return "rocket_std_collections_contains";
    case Intrinsic::CollectionsFind: return "rocket_std_collections_find";
    case Intrinsic::CollectionsFilterEqual: return "rocket_std_collections_filter_equal";
    case Intrinsic::CollectionsSortInt: return "rocket_std_collections_sort_int";
    case Intrinsic::CollectionsSortFloat: return "rocket_std_collections_sort_float";
    case Intrinsic::CollectionsSortChar: return "rocket_std_collections_sort_char";
    case Intrinsic::CollectionsSortString: return "rocket_std_collections_sort_string";
    case Intrinsic::CollectionsMapHash: return "rocket_std_collections_map_hash";
    case Intrinsic::CollectionsFoldSumInt: return "rocket_std_collections_fold_sum_int";
    case Intrinsic::CollectionsFoldSumFloat: return "rocket_std_collections_fold_sum_float";
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
    case Intrinsic::FileReadBinary: return "rocket_std_file_read_binary";
    case Intrinsic::FileWriteBinary: return "rocket_std_file_write_binary";
    case Intrinsic::FileAppendBinary: return "rocket_std_file_append_binary";
    case Intrinsic::BinaryFromString: return "rocket_std_binary_from_string";
    case Intrinsic::BinaryToString: return "rocket_std_binary_to_string";
    case Intrinsic::BinaryLength: return "rocket_std_binary_length";
    case Intrinsic::BinarySlice: return "rocket_std_binary_slice";
    case Intrinsic::BinaryReadU8: return "rocket_std_binary_read_u8";
    case Intrinsic::BinaryReadU16Le: return "rocket_std_binary_read_u16_le";
    case Intrinsic::BinaryReadU32Le: return "rocket_std_binary_read_u32_le";
    case Intrinsic::BinaryWriteU8: return "rocket_std_binary_write_u8";
    case Intrinsic::BinaryWriteU16Le: return "rocket_std_binary_write_u16_le";
    case Intrinsic::BinaryWriteU32Le: return "rocket_std_binary_write_u32_le";
    case Intrinsic::BinaryConcat: return "rocket_std_binary_concat";
    case Intrinsic::BinaryReadU16Be: return "rocket_std_binary_read_u16_be";
    case Intrinsic::BinaryReadU32Be: return "rocket_std_binary_read_u32_be";
    case Intrinsic::BinaryWriteU16Be: return "rocket_std_binary_write_u16_be";
    case Intrinsic::BinaryWriteU32Be: return "rocket_std_binary_write_u32_be";
    case Intrinsic::StreamOpenReader: return "rocket_std_stream_open_reader";
    case Intrinsic::StreamRead: return "rocket_std_stream_read";
    case Intrinsic::StreamCloseReader: return "rocket_std_stream_close_reader";
    case Intrinsic::StreamOpenWriter: return "rocket_std_stream_open_writer";
    case Intrinsic::StreamWrite: return "rocket_std_stream_write";
    case Intrinsic::StreamFlush: return "rocket_std_stream_flush";
    case Intrinsic::StreamCloseWriter: return "rocket_std_stream_close_writer";
    case Intrinsic::UnicodeScalarCount: return "rocket_std_unicode_scalar_count";
    case Intrinsic::UnicodeScalarAt: return "rocket_std_unicode_scalar_at";
    case Intrinsic::UnicodeFromScalar: return "rocket_std_unicode_from_scalar";
    case Intrinsic::UnicodeNormalizeNfc: return "rocket_std_unicode_normalize_nfc";
    case Intrinsic::UnicodeNormalizeNfd: return "rocket_std_unicode_normalize_nfd";
    case Intrinsic::UnicodeGraphemeCount: return "rocket_std_unicode_grapheme_count";
    case Intrinsic::UnicodeGraphemeAt: return "rocket_std_unicode_grapheme_at";
    case Intrinsic::RegexIsMatch: return "rocket_std_regex_is_match";
    case Intrinsic::RegexFindAll: return "rocket_std_regex_find_all";
    case Intrinsic::RegexReplaceAll: return "rocket_std_regex_replace_all";
    case Intrinsic::CryptoSecureBytes: return "rocket_std_crypto_secure_bytes";
    case Intrinsic::CryptoSecureInt: return "rocket_std_crypto_secure_int";
    case Intrinsic::CryptoSha256: return "rocket_std_crypto_sha256";
    case Intrinsic::CryptoHmacSha256: return "rocket_std_crypto_hmac_sha256";
    case Intrinsic::CryptoConstantTimeEqual: return "rocket_std_crypto_constant_time_equal";
    case Intrinsic::CryptoVerifySignedFile: return "rocket_std_crypto_verify_signed_file";
    case Intrinsic::NetResolve: return "rocket_std_net_resolve";
    case Intrinsic::NetTcpConnect: return "rocket_std_net_tcp_connect";
    case Intrinsic::NetTcpListen: return "rocket_std_net_tcp_listen";
    case Intrinsic::NetAccept: return "rocket_std_net_accept";
    case Intrinsic::NetSend: return "rocket_std_net_send";
    case Intrinsic::NetReceive: return "rocket_std_net_receive";
    case Intrinsic::NetClose: return "rocket_std_net_close";
    case Intrinsic::NetCancel: return "rocket_std_net_cancel";
    case Intrinsic::NetLocalPort: return "rocket_std_net_local_port";
    case Intrinsic::HttpRequest: return "rocket_std_http_request";
    case Intrinsic::HttpReadRequest: return "rocket_std_http_read_request";
    case Intrinsic::HttpWriteResponse: return "rocket_std_http_write_response";
    case Intrinsic::DateTimeFormatUtc: return "rocket_std_datetime_format_utc";
    case Intrinsic::DateTimeParseUtc: return "rocket_std_datetime_parse_utc";
    case Intrinsic::DateTimeDaysInMonth: return "rocket_std_datetime_days_in_month";
    case Intrinsic::DateTimeWeekday: return "rocket_std_datetime_weekday";
    case Intrinsic::DateTimeLocalOffsetMinutes: return "rocket_std_datetime_local_offset_minutes";
    case Intrinsic::DateTimeTimezoneName: return "rocket_std_datetime_timezone_name";
    case Intrinsic::LogWrite: return "rocket_std_log_write";
    case Intrinsic::LogAppend: return "rocket_std_log_append";
    case Intrinsic::CliHasFlag: return "rocket_std_cli_has_flag";
    case Intrinsic::CliOption: return "rocket_std_cli_option";
    case Intrinsic::CliPositionals: return "rocket_std_cli_positionals";
    case Intrinsic::ConfigGet: return "rocket_std_config_get";
    case Intrinsic::ConfigLoad: return "rocket_std_config_load";
    case Intrinsic::CompressionXpressCompress: return "rocket_std_compression_xpress_compress";
    case Intrinsic::CompressionXpressDecompress: return "rocket_std_compression_xpress_decompress";
    case Intrinsic::ArchiveTarCreate: return "rocket_std_archive_tar_create";
    case Intrinsic::ArchiveTarList: return "rocket_std_archive_tar_list";
    case Intrinsic::ArchiveTarRead: return "rocket_std_archive_tar_read";
    case Intrinsic::SqliteOpen: return "rocket_std_sqlite_open";
    case Intrinsic::SqliteExecute: return "rocket_std_sqlite_execute";
    case Intrinsic::SqliteQuery: return "rocket_std_sqlite_query";
    case Intrinsic::SqliteClose: return "rocket_std_sqlite_close";
    case Intrinsic::TestingAssert: return "rocket_std_testing_assert";
    case Intrinsic::TestingEqualInt: return "rocket_std_testing_equal_int";
    case Intrinsic::TestingEqualString: return "rocket_std_testing_equal_string";
    case Intrinsic::TestingTempDirectory: return "rocket_std_testing_temp_directory";
    case Intrinsic::TestingFixturePath: return "rocket_std_testing_fixture_path";
    case Intrinsic::TestingCleanupTemp: return "rocket_std_testing_cleanup_temp";
    case Intrinsic::TestingCoverageHit: return "rocket_std_testing_coverage_hit";
    case Intrinsic::TestingCoverageWrite: return "rocket_std_testing_coverage_write";
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
    case Intrinsic::TaskJoin: return "rocket_std_task_join";
    case Intrinsic::TaskIsComplete: return "rocket_std_task_is_complete";
    case Intrinsic::OwnershipDowngrade: return "rocket_std_ownership_downgrade";
    case Intrinsic::OwnershipUpgrade: return "rocket_std_ownership_upgrade";
    case Intrinsic::OwnershipExpired: return "rocket_std_ownership_expired";
    case Intrinsic::BufferThaw: return "rocket_std_buffer_thaw";
    case Intrinsic::BufferLength: return "rocket_std_buffer_length";
    case Intrinsic::BufferCapacity: return "rocket_std_buffer_capacity";
    case Intrinsic::BufferGet: return "rocket_std_buffer_get";
    case Intrinsic::BufferSet: return "rocket_std_buffer_set";
    case Intrinsic::BufferAppend: return "rocket_std_buffer_append";
    case Intrinsic::BufferSlice: return "rocket_std_buffer_slice";
    case Intrinsic::BufferFreeze: return "rocket_std_buffer_freeze";
    case Intrinsic::CancelToken: return "rocket_std_cancel_token";
    case Intrinsic::CancelChild: return "rocket_std_cancel_child";
    case Intrinsic::CancelCurrent: return "rocket_std_cancel_current";
    case Intrinsic::CancelCancel: return "rocket_std_cancel_cancel";
    case Intrinsic::CancelIsCancelled: return "rocket_std_cancel_is_cancelled";
    case Intrinsic::CancelCheck: return "rocket_std_cancel_check";
    case Intrinsic::AsyncTimeDeadlineAfter: return "rocket_std_async_time_deadline_after";
    case Intrinsic::AsyncTimeRemaining: return "rocket_std_async_time_remaining";
    case Intrinsic::AsyncTimeSleep: return "rocket_std_async_time_sleep";
    case Intrinsic::AsyncTimeSleepUntil: return "rocket_std_async_time_sleep_until";
    case Intrinsic::SyncMutex: return "rocket_std_sync_mutex";
    case Intrinsic::SyncLock: return "rocket_std_sync_lock";
    case Intrinsic::SyncGuardGet: return "rocket_std_sync_guard_get";
    case Intrinsic::SyncGuardSet: return "rocket_std_sync_guard_set";
    case Intrinsic::SyncUnlock: return "rocket_std_sync_unlock";
    case Intrinsic::SyncEvent: return "rocket_std_sync_event";
    case Intrinsic::SyncEventSet: return "rocket_std_sync_event_set";
    case Intrinsic::SyncEventReset: return "rocket_std_sync_event_reset";
    case Intrinsic::SyncEventWait: return "rocket_std_sync_event_wait";
    case Intrinsic::SyncAtomicInt: return "rocket_std_sync_atomic_int";
    case Intrinsic::SyncAtomicLoad: return "rocket_std_sync_atomic_load";
    case Intrinsic::SyncAtomicStore: return "rocket_std_sync_atomic_store";
    case Intrinsic::SyncAtomicFetchAdd: return "rocket_std_sync_atomic_fetch_add";
    case Intrinsic::SyncAtomicCompareExchange: return "rocket_std_sync_atomic_compare_exchange";
    case Intrinsic::SyncOnce: return "rocket_std_sync_once";
    case Intrinsic::SyncOnceSet: return "rocket_std_sync_once_set";
    case Intrinsic::SyncOnceGet: return "rocket_std_sync_once_get";
    case Intrinsic::ChannelBounded: return "rocket_std_channel_bounded";
    case Intrinsic::ChannelUnbounded: return "rocket_std_channel_unbounded";
    case Intrinsic::ChannelSender: return "rocket_std_channel_sender";
    case Intrinsic::ChannelReceiver: return "rocket_std_channel_receiver";
    case Intrinsic::ChannelCloneSender: return "rocket_std_channel_clone_sender";
    case Intrinsic::ChannelCloneReceiver: return "rocket_std_channel_clone_receiver";
    case Intrinsic::ChannelSend: return "rocket_std_channel_send";
    case Intrinsic::ChannelReceive: return "rocket_std_channel_receive";
    case Intrinsic::ChannelCloseSender: return "rocket_std_channel_close_sender";
    case Intrinsic::ChannelCloseReceiver: return "rocket_std_channel_close_receiver";
    case Intrinsic::AsyncFileRead: return "rocket_std_async_file_read";
    case Intrinsic::AsyncFileWrite: return "rocket_std_async_file_write";
    case Intrinsic::TaskGroup: return "rocket_std_task_group";
    case Intrinsic::TaskGroupJoin: return "rocket_std_task_group_join";
    case Intrinsic::AsyncNetConnect: return "rocket_std_async_net_connect";
    case Intrinsic::AsyncNetAccept: return "rocket_std_async_net_accept";
    case Intrinsic::AsyncNetReceive: return "rocket_std_async_net_receive";
    case Intrinsic::AsyncNetSend: return "rocket_std_async_net_send";
    case Intrinsic::AsyncProcessRun: return "rocket_std_async_process_run";
    case Intrinsic::ThreadSpawn: return "rocket_std_thread_spawn";
    case Intrinsic::ThreadJoin: return "rocket_std_thread_join";
    case Intrinsic::ThreadDetach: return "rocket_std_thread_detach";
    case Intrinsic::ThreadIsComplete: return "rocket_std_thread_is_complete";
    case Intrinsic::TaskCancel: return "rocket_std_task_cancel";
    case Intrinsic::TaskGroupCancel: return "rocket_std_task_group_cancel";
    case Intrinsic::SyncOnceEmpty: return "rocket_std_sync_once_empty";
    case Intrinsic::TargetAlias: return "rocket_std_target_alias";
    case Intrinsic::TargetTriple: return "rocket_std_target_triple";
    case Intrinsic::TargetOs: return "rocket_std_target_os";
    case Intrinsic::TargetArchitecture: return "rocket_std_target_architecture";
    case Intrinsic::TargetEnvironment: return "rocket_std_target_environment";
    case Intrinsic::TargetPointerWidth: return "rocket_std_target_pointer_width";
    case Intrinsic::TargetEndianness: return "rocket_std_target_endianness";
    case Intrinsic::TargetHasFeature: return "rocket_std_target_has_feature";
    default: return nullptr;
    }
  }

  llvm::Type* standardRuntimeType(const Type& type) {
    if (type == Type::Unit) return llvm::Type::getVoidTy(context_);
    if (type == Type::Bool) return llvm::Type::getInt8Ty(context_);
    return valueType(type);
  }

  llvm::Value* lowerStandard(const MirRvalue& value,
                             const std::vector<llvm::AllocaInst*>& locals,
                             std::string& error) {
    const HirSymbol& symbol = mir_.symbols[value.callee];
    const char* fixedRuntimeName = standardRuntimeName(symbol.intrinsic);
    if (!fixedRuntimeName) {
      error = "unknown standard-library intrinsic reached LLVM lowering";
      return nullptr;
    }
    std::string runtimeName = fixedRuntimeName;
    if (symbol.intrinsic == Intrinsic::CollectionsAppend ||
        symbol.intrinsic == Intrinsic::CollectionsInsert ||
        symbol.intrinsic == Intrinsic::BufferSet ||
        symbol.intrinsic == Intrinsic::BufferAppend) {
      const Type element = collectionElementType(value.arguments[0].type);
      const Type resolvedElement = isUniqueBufferType(value.arguments[0].type)
                                       ? value.arguments[0].type.arguments[0]
                                       : element;
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(resolvedElement);
    }
    if (symbol.intrinsic == Intrinsic::BufferGet) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.type);
    }
    if (symbol.intrinsic == Intrinsic::TaskGroup) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.type.arguments.at(0));
    }
    if (symbol.intrinsic == Intrinsic::SyncMutex ||
        symbol.intrinsic == Intrinsic::SyncOnce ||
        symbol.intrinsic == Intrinsic::SyncOnceEmpty) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.arguments.at(0).type);
    }
    if (symbol.intrinsic == Intrinsic::SyncGuardGet) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.type);
    }
    if (symbol.intrinsic == Intrinsic::SyncGuardSet ||
        symbol.intrinsic == Intrinsic::SyncOnceSet ||
        symbol.intrinsic == Intrinsic::ChannelSend) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.arguments.at(1).type);
    }
    if (symbol.intrinsic == Intrinsic::SyncOnceGet) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.type.arguments.at(0));
    }
    if (symbol.intrinsic == Intrinsic::CollectionsMapFind ||
        symbol.intrinsic == Intrinsic::CollectionsMapGet ||
        symbol.intrinsic == Intrinsic::CollectionsSetContains ||
        symbol.intrinsic == Intrinsic::CollectionsContains ||
        symbol.intrinsic == Intrinsic::CollectionsFind ||
        symbol.intrinsic == Intrinsic::CollectionsFilterEqual) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.arguments[1].type);
    }
    if (symbol.intrinsic == Intrinsic::CollectionsHash) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(value.arguments[0].type);
    }
    if (symbol.intrinsic == Intrinsic::CollectionsMapHash) {
      runtimeName += "_";
      runtimeName += runtimeElementSuffix(collectionElementType(value.arguments[0].type));
    }
    std::vector<llvm::Type*> parameterTypes;
    std::vector<llvm::Value*> arguments;
    for (const auto& argument : value.arguments) {
      llvm::Value* lowered = lowerOperand(argument, locals, error);
      if (!lowered) return nullptr;
      llvm::Type* runtimeType = standardRuntimeType(argument.type);
      if (argument.type == Type::Bool)
        lowered = builder_.CreateZExt(lowered, runtimeType, "std.bool");
      parameterTypes.push_back(runtimeType);
      arguments.push_back(lowered);
    }
    llvm::FunctionCallee function = module_->getOrInsertFunction(
        runtimeName,
        llvm::FunctionType::get(standardRuntimeType(value.type), parameterTypes, false));
    llvm::CallInst* call = builder_.CreateCall(function, arguments,
                                                value.type == Type::Unit ? "" : "std.call");
    if (value.type == Type::Unit) return zero(Type::Unit);
    if (value.type == Type::Bool)
      return builder_.CreateTrunc(call, llvm::Type::getInt1Ty(context_), "std.bool.result");
    return call;
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

    llvm::Type* int32 = llvm::Type::getInt32Ty(context_);
    llvm::Type* pointer = llvm::PointerType::getUnqual(context_);
    auto* signature = llvm::FunctionType::get(int32, {int32, pointer}, false);
    auto* entrypoint = llvm::Function::Create(signature, llvm::Function::ExternalLinkage,
                                               "main", *module_);
    llvm::BasicBlock* entry = llvm::BasicBlock::Create(context_, "entry", entrypoint);
    builder_.SetInsertPoint(entry);
    auto argument = entrypoint->arg_begin();
    llvm::Value* count = &*argument++;
    llvm::Value* arguments = &*argument;
    count->setName("argc");
    arguments->setName("argv");
    llvm::FunctionCallee setArguments = module_->getOrInsertFunction(
        "rocket_std_process_set_arguments",
        llvm::FunctionType::get(llvm::Type::getVoidTy(context_), {int32, pointer}, false));
    builder_.CreateCall(setArguments, {count, arguments});
    llvm::Value* result = builder_.CreateCall(rocketMain, {}, "rocket.main");
    builder_.CreateRet(builder_.CreateTrunc(result, int32, "exit"));
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
  Target target_;
  llvm::LLVMContext context_;
  std::unique_ptr<llvm::Module> module_;
  llvm::IRBuilder<> builder_;
  std::vector<llvm::Function*> functions_;
  std::vector<llvm::Function*> callbackWrappers_;
  std::vector<llvm::Function*> taskThunks_;
  std::vector<llvm::DISubprogram*> subprograms_;
  std::unique_ptr<llvm::TargetMachine> targetMachine_;
  std::unique_ptr<llvm::DIBuilder> debugBuilder_;
  llvm::DICompileUnit* compileUnit_ = nullptr;
  std::map<std::string, llvm::DIFile*> debugFiles_;
  bool debugInfo_ = false;
  bool debugOptimized_ = false;
  bool coverage_ = false;
  bool profiling_ = false;
};

} // namespace

bool generateLlvmIr(const MirModule& module, bool optimize, std::string& output,
                    std::string& error) {
  TargetError targetError;
  const auto host = detectHostTarget(targetError);
  if (!host) {
    error = targetError.message;
    return false;
  }
  return generateLlvmIr(module, optimize, *host, output, error);
}

bool generateLlvmIr(const MirModule& module, bool optimize, const Target& target,
                    std::string& output, std::string& error) {
  output.clear();
  error.clear();
  ModuleLowerer lowerer(module, false, false, false, target);
  if (!lowerer.prepare(optimize, error)) return false;
  output = lowerer.ir();
  return true;
}

bool emitLlvmFile(const MirModule& module, bool optimize, LlvmFileType fileType,
                  const std::filesystem::path& outputPath, std::string& error,
                  bool debugInfo, bool coverage, bool profiling) {
  TargetError targetError;
  const auto host = detectHostTarget(targetError);
  if (!host) {
    error = targetError.message;
    return false;
  }
  return emitLlvmFile(module, optimize, fileType, *host, outputPath, error,
                      debugInfo, coverage, profiling);
}

bool emitLlvmFile(const MirModule& module, bool optimize, LlvmFileType fileType,
                  const Target& target,
                  const std::filesystem::path& outputPath, std::string& error,
                  bool debugInfo, bool coverage, bool profiling) {
  error.clear();
  ModuleLowerer lowerer(module, debugInfo, coverage, profiling, target);
  if (!lowerer.prepare(optimize, error)) return false;
  return lowerer.emit(fileType, outputPath, error);
}

} // namespace rocket
