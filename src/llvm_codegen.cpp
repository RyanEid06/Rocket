#include "llvm_codegen.h"

#include <llvm/ADT/StringRef.h>
#include <llvm/IR/BasicBlock.h>
#include <llvm/IR/Constants.h>
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
    switch (type.kind) {
    case TypeKind::Int: return llvm::Type::getInt64Ty(context_);
    case TypeKind::Float: return llvm::Type::getDoubleTy(context_);
    case TypeKind::Bool: return llvm::Type::getInt1Ty(context_);
    case TypeKind::Char: return llvm::Type::getInt8Ty(context_);
    case TypeKind::String:
    case TypeKind::Array:
    case TypeKind::Slice:
    case TypeKind::Struct:
    case TypeKind::Enum:
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
    case TypeKind::Struct:
    case TypeKind::Enum:
      return llvm::ConstantPointerNull::get(llvm::PointerType::getUnqual(context_));
    case TypeKind::TypeParameter:
    case TypeKind::Invalid: break;
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
    case TypeKind::Struct:
    case TypeKind::Enum:
      error = "aggregate constant reached LLVM lowering";
      return nullptr;
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

    if (mir_.symbols[value.callee].kind == SymbolKind::BuiltinFunction) {
      if (mir_.symbols[value.callee].intrinsic == Intrinsic::Print)
        return lowerPrint(value, locals, error);
      return lowerStandard(value, locals, error);
    }

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

  static std::uint32_t runtimeElementKind(Type element) {
    switch (element.kind) {
    case TypeKind::Int: return 1;
    case TypeKind::Float: return 2;
    case TypeKind::Bool: return 3;
    case TypeKind::Char: return 4;
    case TypeKind::String: return 5;
    case TypeKind::Array:
    case TypeKind::Slice:
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
    case Intrinsic::StringParseInt: return "rocket_std_string_parse_int";
    case Intrinsic::StringFromInt: return "rocket_std_string_from_int";
    case Intrinsic::CollectionsLength: return "rocket_std_collections_length";
    case Intrinsic::CollectionsReverse: return "rocket_std_collections_reverse";
    case Intrinsic::CollectionsJoin: return "rocket_std_collections_join";
    case Intrinsic::FileReadText: return "rocket_std_file_read_text";
    case Intrinsic::FileWriteText: return "rocket_std_file_write_text";
    case Intrinsic::FileAppendText: return "rocket_std_file_append_text";
    case Intrinsic::FileExists: return "rocket_std_file_exists";
    case Intrinsic::FileRemove: return "rocket_std_file_remove";
    case Intrinsic::FileList: return "rocket_std_file_list";
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
    case Intrinsic::ProcessEnvironment: return "rocket_std_process_environment";
    case Intrinsic::ProcessWorkingDirectory: return "rocket_std_process_working_directory";
    case Intrinsic::TimeUnixMilliseconds: return "rocket_std_time_unix_milliseconds";
    case Intrinsic::TimeMonotonicMilliseconds: return "rocket_std_time_monotonic_milliseconds";
    case Intrinsic::TimeSleepMilliseconds: return "rocket_std_time_sleep_milliseconds";
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
    const char* runtimeName = standardRuntimeName(symbol.intrinsic);
    if (!runtimeName) {
      error = "unknown standard-library intrinsic reached LLVM lowering";
      return nullptr;
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
