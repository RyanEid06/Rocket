#pragma once

#include "mir.h"

#include <ostream>
#include <string>

namespace rocket {

class BootstrapCodeGenerator {
public:
  explicit BootstrapCodeGenerator(const MirModule& module) : module_(module) {}
  std::string generate() const;

private:
  void emitFunction(std::ostream& out, const MirFunction& function) const;
  void emitInstruction(std::ostream& out, const MirInstruction& instruction) const;
  void emitTerminator(std::ostream& out, const MirTerminator& terminator,
                      Type functionResult) const;
  void emitRvalue(std::ostream& out, const MirRvalue& value) const;
  void emitOperand(std::ostream& out, const MirOperand& operand) const;
  static std::string cppType(Type type);
  static std::string escaped(const std::string& text);
  static std::string escapedCharacter(const std::string& text);
  std::string functionName(SymbolId symbol) const;
  static std::string localName(MirLocalId local);

  const MirModule& module_;
};

} // namespace rocket
