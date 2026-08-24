#pragma once

#include "mir.h"
#include "target.h"

#include <filesystem>
#include <string>

namespace rocket {

enum class LlvmFileType { Object, Assembly };

// Lowers verified scalar MIR to a host-targeted LLVM module. When optimize is
// true, LLVM's standard O2 pipeline runs before the module is returned.
bool generateLlvmIr(const MirModule& module, bool optimize, std::string& output,
                    std::string& error);
bool generateLlvmIr(const MirModule& module, bool optimize, const Target& target,
                    std::string& output, std::string& error);

// Lowers and emits a host object or assembly file through LLVM's target
// machine. The output is suitable for the pinned Windows x64 linker toolchain.
bool emitLlvmFile(const MirModule& module, bool optimize, LlvmFileType fileType,
                  const std::filesystem::path& outputPath, std::string& error,
                  bool debugInfo = false, bool coverage = false,
                  bool profiling = false);
bool emitLlvmFile(const MirModule& module, bool optimize, LlvmFileType fileType,
                  const Target& target,
                  const std::filesystem::path& outputPath, std::string& error,
                  bool debugInfo = false, bool coverage = false,
                  bool profiling = false);

} // namespace rocket
